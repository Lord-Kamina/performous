#include "screen_paths.hh"

#include "configuration.hh"
#include "controllers.hh"
#include "theme.hh"
#include "audio.hh"
#include "i18n.hh"
#include <boost/thread.hpp>
#include <boost/bind.hpp>

namespace {
	const float yoff = 0.18; // Offset from center where to place top row
	const float xoff = 0.45; // Offset from middle where to place first column
}

ScreenPaths::ScreenPaths(std::string const& name, Audio& audio): Screen(name), m_audio(audio) {}

void ScreenPaths::enter() {
	m_theme.reset(new ThemeAudioDevices());
	m_txtinp.text.clear();

	// FIXME: Temp error message
	Game::getSingletonPtr()->dialog(
		_("This tool is not yet available.\n"
		  "Configure paths by adding them\n"
		  "as command line parameters and\n"
		  "then save them in configuration menu."));
}

void ScreenPaths::exit() { m_theme.reset(); }

void ScreenPaths::manageEvent(SDL_Event event) {
	if (event.type == SDL_KEYDOWN) {
		return; // FIXME: Remove
		SDL_Keycode key = event.key.keysym.sym;
		uint16_t modifier = event.key.keysym.mod;
		if (m_txtinp.process(event.key.keysym)) /* Nop */ ;
		// Reset to defaults
		else if (key == SDLK_r && modifier & KMOD_CTRL) {
			config["paths/songs"].reset(modifier & KMOD_ALT);
			config["paths/system"].reset(modifier & KMOD_ALT);
			// TODO: Save
		}
	}
}

void ScreenPaths::manageEvent(input::NavEvent const& ev) {
	Game* gm = Game::getSingletonPtr();
	if (ev.button == input::NAV_CANCEL) {
		if (m_txtinp.text.empty()) gm->activateScreen("Intro");
		else m_txtinp.text.clear();
	}
	else if (ev.button == input::NAV_PAUSE) m_audio.togglePause();
	else if (ev.button == input::NAV_START) { 
		// TODO: Save config
		gm->activateScreen("Intro");
	}
}

void ScreenPaths::draw() {
	if (!Game::getSingletonPtr()->isDialogOpen())
		Game::getSingletonPtr()->activateScreen("Intro"); // FIXME: Remove

	m_theme->bg.draw();

	// Key help
	m_theme->comment_bg.dimensions.stretch(1.0, 0.025).middle().screenBottom(-0.054);
	m_theme->comment_bg.draw();
	m_theme->comment.dimensions.left(-0.48).screenBottom(-0.067);
	m_theme->comment.draw(_("Press any key to exit."));
	// Additional info
	#ifdef _WIN32
	m_theme->comment_bg.dimensions.middle().screenBottom(-0.01);
	m_theme->comment_bg.draw();
	m_theme->comment.dimensions.left(-0.48).screenBottom(-0.023);
	m_theme->comment.draw(_("Windows users can also use ConfigureSongDirectory.bat script."));
	#endif
}