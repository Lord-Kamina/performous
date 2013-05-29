#include "log.hh"

#include <stdexcept>
#include <iostream>

#include <boost/iostreams/stream_buffer.hpp>
#include <boost/regex.hpp>
#include <boost/thread/mutex.hpp>

#ifndef NDEBUG
#	include <boost/algorithm/string/trim.hpp>
#endif

/** \file
 * \brief The std::clog logger.
 *
 * The classification is \e discretionary, meaning that you can specify any message class you want but there are only 4 recommended classes: \p debug, \p info, \p warning and \p error.
 * \attention Each new line (ended by <tt>std::endl</tt>) must be prefixed with the message classification. New lines by <tt>"\n"</tt> will belong to the previous line as far as the filter is concerned.
 *
 * The default log level is specified by logger::default_log_level.
 *
 * General message format: <tt>subsystem/class: message</tt>
 *
 * Example:
 * \code
 * std::clog << "subsystem/info: Here's an info message from subsystem" << std::endl;
 * \endcode
 *
 * \todo setup and teardown could probably be RAII-ized away by making a class and having fun by making them dtor/ctor. (it'd be nice if we could make it init itself before any other component that might use it does)
 **/

namespace logger {
	/** \internal
	 * Guard to ensure we're atomically printing to cout/cerr.
	 * \attention This only guards from multiple clog interleaving, not other console I/O.
	 */
	boost::mutex log_lock;

	/** \internal The implementation of the stream filter that handles the message filtering. **/
	class VerboseMessageSink : public boost::iostreams::sink {
	  public:
		std::streamsize write(const char* s, std::streamsize n);
	};

	// defining them in main() causes segfault at exit as they apparently got free'd before we're done using them
	static boost::iostreams::stream_buffer<VerboseMessageSink> sb; //!< \internal
	static VerboseMessageSink vsm; //!< \internal

	//! \internal used to store the default/original clog buffer.
	static std::streambuf* default_ClogBuf = nullptr;

	//! \internal The log level regular expression.
	static boost::regex *level_regex = nullptr;

	std::streamsize VerboseMessageSink::write(const char* s, std::streamsize n) {
		// Note: s is *not* a c-string, thus we must stop after n chars.
		std::string line(s, n);

		// We only check the message for debug builds.
#		ifndef NDEBUG
		// this is a _very liberal_ regexp, we could use this to
		// enforce stricter restrictions like:
		//   [\w]+/(info|warning|error|unknown):
		static const boost::regex prefix_regex("^[^/]+/[^\\:]+:", boost::regex_constants::match_continuous);
		if(!boost::regex_search(line, prefix_regex)){
			std::string tmp = "<!> Bad log prefix detected, log string is: \"" + boost::algorithm::trim_right_copy(line) + std::string("\"");

			//throw std::runtime_error(tmp);
			// It seems throwing an exception here doesn't work,
			// so for now, do the next best thing: make some noise
			{
				boost::mutex::scoped_lock l(log_lock);
				std::cerr << tmp << std::endl;
			}
			return n;
		}
#		endif

		// extract the prefix (this assumes a valid prefix exists)
		// (silently ignore prefixes missing ':', it's caught in debug builds)
		size_t i=line.find(':');
		if(i == std::string::npos) return n;

		std::string prefix(line, 0, i);

		if(boost::regex_match(prefix, *level_regex)){
			boost::mutex::scoped_lock l(log_lock);
			std::cout << line << std::flush;
		}

		return n;
	}

	void setup(const std::string &level_regexp_str){
		if(default_ClogBuf != nullptr) throw std::runtime_error("Internal logger error #0");
		if(level_regex != nullptr) throw std::runtime_error("Internal logger error #1");

		// since we get the regexp from users, we must check it for sanity
		// (or live with "undefined behaviour" on exotic regexps)
		// This regex matching regexp is fairly liberal. See VerboseMessageSink::write() for ideas on stricter use.
		static const boost::regex prefix_regex("^[^/[:blank:]]+/[^[:blank:]]", boost::regex_constants::match_continuous);
		if(!boost::regex_search(level_regexp_str, prefix_regex)){
			throw std::runtime_error("Bad log level.");
		}

		level_regex = new boost::regex(level_regexp_str);

		{ // scope here lest we might dead lock (if showing .*/info).
			boost::mutex::scoped_lock l(log_lock);

			sb.open(vsm);
			default_ClogBuf = std::clog.rdbuf();
			std::clog.rdbuf(&sb);
		}
		std::clog << "logger/info: Log level is: " << level_regexp_str << std::endl;
	}

	// Note: not calling this at exit time introduces a use/dtor-race, which may cause a crash.
	void teardown(){
		if(default_ClogBuf == nullptr) throw std::runtime_error("Internal logger error #2");
		if(level_regex == nullptr) throw std::runtime_error("Internal logger error #3");
		boost::mutex::scoped_lock l(log_lock);

		std::clog.rdbuf(default_ClogBuf);
		sb.close();
		default_ClogBuf=nullptr;
		delete level_regex;
		level_regex = nullptr;
	}

	void log_hh_test(){
#		ifndef NDEBUG
		using namespace std;
		clog << "BEGIN TEST" << endl;

		// logger not initialized, clog as per C++ defaults
		clog << "This message is unhandled." << endl;

		try{
			setup("bad regexp");
			cout << "Bad, bad regex not caught!" << endl;
			teardown();
		}catch (/*std::runtime_error e*/...){
			cout << "Good, caught bad regexp" << endl;
		}

#		define LOG_HH_EXPECT_LOG_EXCEPTION(STR) try {				\
			clog << STR << endl;									\
			cout << "FAIL: " << STR << endl;						\
		} catch(...){ /*cout << "PASS: " << STR << endl;*/ }
#		define LOG_HH_EXPECT_LOG_NOTEXCEPT(STR) try {					\
			clog << STR << endl;										\
			/*cout << "PASS: " << STR << endl;*/						\
		} catch(...){ cout << "FAIL: " << STR << endl; }

		setup(".*/.*");

		// Ok messages
		LOG_HH_EXPECT_LOG_NOTEXCEPT("core/info: Info class message.");
		LOG_HH_EXPECT_LOG_NOTEXCEPT("core/warning: Warning class message.");
		LOG_HH_EXPECT_LOG_NOTEXCEPT("core/error: Error class message.");

		teardown();
		setup(".*/(error|info)");

		// Ok messages
		LOG_HH_EXPECT_LOG_NOTEXCEPT("core/info: Info class message.");
		LOG_HH_EXPECT_LOG_NOTEXCEPT("core/warning: Warning class message.");
		LOG_HH_EXPECT_LOG_NOTEXCEPT("core/error: Error class message.");

		teardown();
		setup(".*/error");

		// Ok messages
		LOG_HH_EXPECT_LOG_NOTEXCEPT("core/info: Info class message.");
		LOG_HH_EXPECT_LOG_NOTEXCEPT("core/warning: Warning class message.");
		LOG_HH_EXPECT_LOG_NOTEXCEPT("core/error: Error class message.");

		// Bad messages:
		LOG_HH_EXPECT_LOG_NOTEXCEPT("core/badclass: Message with invalid class.");

		// Some malformed messages
		// add any messages that we find causes problems/bugs (if any)

		LOG_HH_EXPECT_LOG_EXCEPTION("Message with no prefix");
		LOG_HH_EXPECT_LOG_EXCEPTION("core/ Message with bad prefix");
		LOG_HH_EXPECT_LOG_EXCEPTION("core/: Message with bad prefix");
		LOG_HH_EXPECT_LOG_EXCEPTION("/: Message with bad prefix");
		LOG_HH_EXPECT_LOG_EXCEPTION("core/: Message with bad prefix");
		LOG_HH_EXPECT_LOG_EXCEPTION("/bad Message with bad prefix");

		teardown();
		setup(".*/error");
		// just checking this case:

		// no errors here please
		clog << "core/error: Line 1,"; clog << " still Line 1" << endl;
		clog << "core/info: Line 1,"; clog << " still Line 1" << endl;

		// both should error on Line 3
		clog << "core/error: Line 1\nLine 2" << endl << "Line 3" << endl;
		clog << "core/info: Line 1\nLine 2" << endl << "Line 3" << endl;

#		undef LOG_HH_EXPECT_LOG_NOTEXCEPT
#		undef LOG_HH_EXPECT_LOG_EXCEPTION

		teardown();

		// logger not initialized, clog as per C++ defaults
		clog << "This message is unhandled." << endl;

		clog << "END   TEST" << endl;
#		endif
	}

}
