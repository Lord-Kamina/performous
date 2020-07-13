#pragma once

#include "animvalue.hh"
#include "menu.hh"
#include "screen.hh"
#include "theme.hh"

class Audio;
class ThemeIntro;
class SvgTxtTheme;
class MenuOption;

/// intro screen
class ScreenIntro : public Screen {
  public:
	/// constructor
	ScreenIntro(std::string const& name, Audio& audio);
	void enter();
	void exit();
	void reloadGL();
	void manageEvent(SDL_Event event);
	void manageEvent(input::NavEvent const& event);
	void draw();

  private:
	void draw_menu_options();
	void draw_webserverNotice();
	void populateMenu();
	std::shared_ptr<SvgTxtTheme> getTextObject(std::string const& txt);

	Audio& m_audio;
	std::unique_ptr<ThemeIntro> theme;
	Menu m_menu;
	bool m_first;
	bool m_drawNotice = false;
	AnimValue m_selAnim;
	AnimValue m_submenuAnim;
	AnimValue m_webserverNoticeTimeout;
	float m_lineSpacing = -1.0f;
	float m_lineHeight = -1.0f;
	unsigned m_linesOnScreen = 0;
	unsigned m_animationTargetLines = 0;
	unsigned m_maxLinesOnScreen = 0;
	int webserversetting = 0;
	const float m_options_x = 0.35f;
	const float m_start_y = -0.3f;
	const unsigned short showOpts = 5; // Show at most 5 options simultaneously
};
