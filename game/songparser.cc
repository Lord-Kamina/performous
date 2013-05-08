#include "songparser.hh"

#include <fstream>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>


namespace SongParserUtil {
	void assign(int& var, std::string const& str) {
		try {
			var = boost::lexical_cast<int>(str);
		} catch (...) {
			throw std::runtime_error("\"" + str + "\" is not valid integer value");
		}
	}
	void assign(unsigned& var, std::string const& str) {
		try {
			var = boost::lexical_cast<int>(str);
		} catch (...) {
			throw std::runtime_error("\"" + str + "\" is not valid unsigned integer value");
		}
	}
	void assign(double& var, std::string str) {
		std::replace(str.begin(), str.end(), ',', '.'); // Fix decimal separators
		try {
			var = boost::lexical_cast<double>(str);
		} catch (...) {
			throw std::runtime_error("\"" + str + "\" is not valid floating point value");
		}
	}
	void assign(bool& var, std::string const& str) {
		if (str == "YES" || str == "yes" || str == "1") var = true;
		else if (str == "NO" || str == "no" || str == "0") var = false;
		else throw std::runtime_error("Invalid boolean value: " + str);
	}
	void eraseLast(std::string& s, char ch) {
		if (!s.empty() && *s.rbegin() == ch) s.erase(s.size() - 1);
	}
}


/// constructor
SongParser::SongParser(Song& s) try:
  m_song(s),
  m_linenum(),
  m_relative(),
  m_gap(),
  m_bpm(),
  m_prevtime(),
  m_prevts(),
  m_relativeShift(),
  m_tsPerBeat(),
  m_tsEnd()
{
	enum { NONE, TXT, XML, INI, SM } type = NONE;
	// Read the file, determine the type and do some initial validation checks
	{
		std::ifstream f((s.path + s.filename).c_str(), std::ios::binary);
		if (!f.is_open()) throw SongParserException(s, "Could not open song file", 0);
		f.seekg(0, std::ios::end);
		size_t size = f.tellg();
		if (size < 10 || size > 100000) throw SongParserException(s, "Does not look like a song file (wrong size)", 1, true);
		f.seekg(0);
		std::vector<char> data(size);
		if (!f.read(&data[0], size)) throw SongParserException(s, "Unexpected I/O error", 0);
		if (smCheck(data)) type = SM;
		else if (txtCheck(data)) type = TXT;
		else if (iniCheck(data)) type = INI;
		else if (xmlCheck(data)) type = XML;
		else throw SongParserException(s, "Does not look like a song file (wrong header)", 1, true);
		m_ss.write(&data[0], size);
	}
	convertToUTF8(m_ss, s.path + s.filename);
	// Header already parsed?
	if (s.loadStatus == Song::HEADER) {
		if (type == TXT) txtParse();
		else if (type == INI) iniParse();
		else if (type == XML) xmlParse();
		else if (type == SM) smParse();
		finalize(); // Do some adjusting to the notes
		s.loadStatus = Song::FULL;
		return;
	}
	// Parse only header to speed up loading and conserve memory
	if (type == TXT) txtParseHeader();
	else if (type == INI) iniParseHeader();
	else if (type == XML) xmlParseHeader();
	else if (type == SM) { smParseHeader(); s.dropNotes(); } // Hack: drop notes here
	// Default for preview position if none was specified in header
	if (s.preview_start != s.preview_start) s.preview_start = (type == INI ? 5.0 : 30.0);  // 5 s for band mode, 30 s for others

	// Remove bogus entries
	if (!boost::filesystem::exists(m_song.path + m_song.cover)) m_song.cover = "";
	if (!boost::filesystem::exists(m_song.path + m_song.background)) m_song.background = "";
	if (!boost::filesystem::exists(m_song.path + m_song.video)) m_song.video = "";

	// In case no images/videos were specified, try to guess them
	if (m_song.cover.empty() || m_song.background.empty() || m_song.video.empty()) {
		boost::regex coverfile("((cover|album|label|\\[co\\])\\.(png|jpeg|jpg|svg))$", boost::regex_constants::icase);
		boost::regex backgroundfile("((background|bg||\\[bg\\])\\.(png|jpeg|jpg|svg))$", boost::regex_constants::icase);
		boost::regex videofile("(.*\\.(avi|mpg|mpeg|flv|mov|mp4))$", boost::regex_constants::icase);
		boost::cmatch match;

		for (boost::filesystem::directory_iterator dirIt(s.path), dirEnd; dirIt != dirEnd; ++dirIt) {
			boost::filesystem::path p = dirIt->path();
			std::string name = p.filename().string(); // File basename
			if (m_song.cover.empty() && regex_match(name.c_str(), match, coverfile)) m_song.cover = name;
			else if (m_song.background.empty() && regex_match(name.c_str(), match, backgroundfile)) m_song.background = name;
			else if (m_song.video.empty() && regex_match(name.c_str(), match, videofile)) m_song.video = name;
		}
	}
	s.loadStatus = Song::HEADER;
	if (type == TXT) txtParse();
	else if (type == INI) iniParse();
	else if (type == XML) xmlParse();
	else if (type == SM) smParse();
	finalize(); // Do some adjusting to the notes
	s.loadStatus = Song::FULL;
} catch (std::runtime_error& e) {
	throw SongParserException(m_song, e.what(), m_linenum);
} catch (std::exception& e) {
	throw SongParserException(m_song, "Internal error: " + std::string(e.what()), m_linenum);
}

void SongParser::resetNoteParsingState() {
	m_prevtime = 0;
	m_prevts = 0;
	m_relativeShift = 0;
	m_tsPerBeat = 0;
	m_tsEnd = 0;
	m_bpms.clear();
	if (m_bpm != 0.0) addBPM(0, m_bpm);
}

void SongParser::vocalsTogether() {
	VocalTracks::iterator togetherIt = m_song.vocalTracks.find("Together");
	if (togetherIt == m_song.vocalTracks.end()) return;
	Notes& together = togetherIt->second.notes;
	if (!together.empty()) return;
	Notes notes;
	// Collect usable vocal tracks
	struct TrackInfo {
		typedef Notes::const_iterator It;
		It it, end;
		TrackInfo(It begin, It end): it(begin), end(end) {}
	};
	std::vector<TrackInfo> tracks;
	for (auto& nt: m_song.vocalTracks) {
		Notes& n = nt.second.notes;
		if (!n.empty()) tracks.push_back(TrackInfo(n.begin(), n.end()));
	}
	if (tracks.empty()) return;
	// Combine notes
	// FIXME: This should do combining on sentence level rather than note-by-note
	TrackInfo* trk = &tracks.front();
	while (trk) {
		Note const& n = *trk->it;
		std::cerr << " " << n.syllable << ": " << n.begin << "-" << n.end << std::endl;
		notes.push_back(n);
		++trk->it;
		trk = NULL;
		// Iterate all tracks past the processed note and find the new earliest track
		for (TrackInfo& trk2: tracks) {
			// Skip until a sentence that begins after the note ended
			while (trk2.it != trk2.end && trk2.it->begin < n.end) ++trk2.it;
			if (trk2.it == trk2.end) continue;
			if (!trk || trk2.it->begin < trk->it->begin) trk = &trk2;
		}
	}
	together.swap(notes);
}

void SongParser::finalize() {
	vocalsTogether();
	for (auto& nt: m_song.vocalTracks) {
		VocalTrack& vocal = nt.second;
		// Remove empty sentences
		{
			Note::Type lastType = Note::NORMAL;
			for (Notes::iterator itn = vocal.notes.begin(); itn != vocal.notes.end();) {
				Note::Type type = itn->type;
				if(type == Note::SLEEP && lastType == Note::SLEEP) {
					std::clog << "songparser/warning: Discarding empty sentence" << std::endl;
					itn = vocal.notes.erase(itn);
				} else {
					++itn;
				}
				lastType = type;
			}
		}
		// Adjust negative notes
		if (vocal.noteMin <= 0) {
			unsigned int shift = (1 - vocal.noteMin / 12) * 12;
			vocal.noteMin += shift;
			vocal.noteMax += shift;
			for (Notes::iterator it = vocal.notes.begin(); it != vocal.notes.end(); ++it) {
				it->note += shift;
				it->notePrev += shift;
			}
		}
		// Set begin/end times
		if (!vocal.notes.empty()) vocal.beginTime = vocal.notes.front().begin, vocal.endTime = vocal.notes.back().end;
		else vocal.beginTime = vocal.endTime = 0.0;
		// Compute maximum score
		double max_score = 0.0;
		for (Notes::iterator itn = vocal.notes.begin(); itn != vocal.notes.end(); ++itn) {
			max_score += itn->maxScore();
		}
		vocal.m_scoreFactor = 1.0 / max_score;
	}
	if (m_tsPerBeat) {
		// Add song beat markers
		for (unsigned ts = 0; ts < m_tsEnd; ts += m_tsPerBeat) m_song.beats.push_back(tsTime(ts));
	}
}
