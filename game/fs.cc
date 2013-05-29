#include "fs.hh"

#include "config.hh"
#include "configuration.hh"
#include "execname.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

fs::path getHomeDir() {
	static fs::path dir;
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		#ifdef _WIN32
		char const* home = getenv("USERPROFILE");
		#else
		char const* home = getenv("HOME");
		#endif
		if (home) dir = home;
	}
	return dir;
}

fs::path getLocaleDir() {
	static fs::path dir;

	if(LOCALEDIR[0] == '/') {
		dir = LOCALEDIR;
	} else {
		dir = plugin::execname().parent_path().parent_path() / LOCALEDIR;
	}

	return dir;
}

fs::path getConfigDir() {
	static fs::path dir;
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		#ifndef _WIN32
		{
			char const* conf = getenv("XDG_CONFIG_HOME");
			if (conf) dir = fs::path(conf) / "performous";
			else dir = getHomeDir() / ".config" / "performous";
		}
		#else
		{
			// Open AppData directory
			std::string str;
			ITEMIDLIST* pidl;
			HRESULT hRes = SHGetSpecialFolderLocation( NULL, CSIDL_APPDATA|CSIDL_FLAG_CREATE , &pidl );
			if (hRes==NOERROR)
			{
				char AppDir[MAX_PATH];
				SHGetPathFromIDList( pidl, AppDir );
				int i;
				for (i = 0; AppDir[i] != '\0'; i++) {
					if (AppDir[i] == '\\') str += '/';
					else str += AppDir[i];
				}
				dir = fs::path(str) / "performous";
			}
		}
		#endif
	}
	return dir;
}

fs::path getDataDir() {
#ifdef _WIN32
	return getConfigDir();  // APPDATA/performous
#else
	fs::path shortDir = "performous";
	fs::path shareDir = SHARED_DATA_DIR;
	char const* xdg_data_home = getenv("XDG_DATA_HOME");
	// FIXME: Should this use "games" or not?
	return (xdg_data_home ? xdg_data_home / shortDir : getHomeDir() / ".local" / shareDir);
#endif
}

fs::path getCacheDir() {
#ifdef _WIN32
	return getConfigDir() / "cache";  // APPDATA/performous
#else
	fs::path shortDir = "performous";
	char const* xdg_cache_home = getenv("XDG_CACHE_HOME");
	// FIXME: Should this use "games" or not?
	return (xdg_cache_home ? xdg_cache_home / shortDir : getHomeDir() / ".cache" / shortDir);
#endif
}

fs::path getThemeDir() {
	std::string theme = config["game/theme"].getEnumName();
	static const std::string defaultTheme = "default";
	if (theme.empty()) theme = defaultTheme;
	return getDataDir() / "themes" / theme;
}

fs::path pathMangle(fs::path const& dir) {
	fs::path ret;
	for (fs::path::const_iterator it = dir.begin(); it != dir.end(); ++it) {
		if (it == dir.begin() && *it == "~") {
			ret = getHomeDir();
			continue;
		}
		ret /= *it;
	}
	return ret;
}

std::string getThemePath(std::string const& filename) {
	std::string theme = config["game/theme"].getEnumName();
	static const std::string defaultTheme = "default";
	if (theme.empty()) theme = defaultTheme;
	// Try current theme and if that fails, try default theme and finally data dir
	try { return getPath(fs::path("themes") / theme / filename); } catch (std::runtime_error&) {}
	if (theme != defaultTheme) try { return getPath(fs::path("themes") / defaultTheme / filename); } catch (std::runtime_error&) {}
	return getPath(filename);
}

std::vector<std::string> getThemes() {
	std::vector<std::string> themes;
	// Search all paths for themes folders and add them
	Paths const& paths = getPaths();
	for (auto p : paths) {
		
		p /= fs::path("themes");
		if (fs::is_directory(p)) {
			// Gather the themes in this folder
			for (fs::directory_iterator dirIt(p), dirEnd; dirIt != dirEnd; ++dirIt) {
				fs::path p2 = dirIt->path();
				if (fs::is_directory(p2)) {
					themes.push_back(p2.filename().string());
				}
			}
		}
	}
	// No duplicates allowed
	std::sort(themes.begin(), themes.end());
	auto last = std::unique(themes.begin(), themes.end());
	themes.erase(last, themes.end());
	return themes;
}

bool isThemeResource(fs::path filename){
	try {
		std::string themefile = getThemePath(filename.filename().string());
		return themefile == filename;
	} catch (...) { return false; }
}

namespace {
	bool pathNotExist(fs::path const& p) {
		if (exists(p)) {
			std::clog << "fs/info: Using data path \"" << p.string() << "\"" << std::endl;
			return false;
		}
		std::clog << "fs/info: Not using \"" << p.string() << "\" (does not exist)" << std::endl;
		return true;
	}
}

std::string getPath(fs::path const& filename) {
	for (auto p: getPaths()) {
		p /= filename;
		if (fs::exists(p)) return p.string();
	}
	throw std::runtime_error("Cannot find file \"" + filename.string() + "\" in any of Performous data folders");
}

Paths const& getPaths(bool refresh) {
	static Paths paths;
	static bool initialized = false;
	if (!initialized || refresh) {
		initialized = true;
		fs::path shortDir = "performous";
		fs::path shareDir = SHARED_DATA_DIR;
		Paths dirs;

		// Adding users data dir
		dirs.push_back(getDataDir());

		// Adding relative path from executable
		dirs.push_back(plugin::execname().parent_path().parent_path() / shareDir);
#ifndef _WIN32
		// Adding XDG_DATA_DIRS
		{
			char const* xdg_data_dirs = getenv("XDG_DATA_DIRS");
			std::istringstream iss(xdg_data_dirs ? xdg_data_dirs : "/usr/local/share/:/usr/share/");
			for (std::string p; std::getline(iss, p, ':'); dirs.push_back(p / shortDir)) {}
		}
#endif
		// Adding paths from config file
		std::vector<std::string> const& confPaths = config["paths/system"].sl();
		std::transform(confPaths.begin(), confPaths.end(), std::inserter(dirs, dirs.end()), pathMangle);
		// Check if they actually exist and print debug
		paths.clear();
		std::remove_copy_if(dirs.begin(), dirs.end(), std::inserter(paths, paths.end()), pathNotExist);
		// Assure that each path appears only once
		paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
	}
	return paths;
}

fs::path getDefaultConfig(fs::path const& configFile) {
	typedef std::vector<std::string> ConfigList;
	ConfigList config_list;
	char const* root = getenv("PERFORMOUS_ROOT");
	if (root) config_list.push_back(std::string(root) + "/" SHARED_DATA_DIR + configFile.string());
	fs::path exec = plugin::execname();
	if (!exec.empty()) config_list.push_back(exec.parent_path().string() + "/../" SHARED_DATA_DIR + configFile.string());
	ConfigList::const_iterator it = std::find_if(config_list.begin(), config_list.end(), static_cast<bool(&)(fs::path const&)>(fs::exists));
	if (it == config_list.end()) {
		throw std::runtime_error("Could not find default config file " + configFile.string());
	}
	return *it;
}

Paths getPathsConfig(std::string const& confOption) {
	Paths ret;
	Paths const& paths = getPaths();
	for (auto const& str: config[confOption].sl()) {
		fs::path p = pathMangle(str);
		if (p.empty()) continue;  // Ignore empty paths
		if (*p.begin() == "DATADIR") {
			// Remove first element
			p = p.string().substr(7);
			// Add all data paths with p appended to them
			for (auto const& path: paths) ret.push_back(path / p);
			continue;
		}
		ret.push_back(p);
	}
	return ret;
}

