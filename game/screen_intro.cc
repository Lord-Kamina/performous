#include "screen_intro.hh"

#include "fs.hh"
#include "glmath.hh"
#include "audio.hh"
#include "i18n.hh"
#include "controllers.hh"
#include "platform.hh"
#include "theme.hh"
#include "menu.hh"

#include <SDL_timer.h>

ScreenIntro::ScreenIntro(std::string const& name, Audio& audio): Screen(name), m_audio(audio), m_menu(Menu()), m_first(true) {
	std::clog << "ScreenIntro/debug: constructing screen... address: " << std::addressof(*this) << std::endl;
}

namespace {
	float test_pos = -0.35f;
}

void ScreenIntro::enter() {
	Game::getSingletonPtr()->showLogo();
	m_audio.playMusic(findFile("menu.ogg"), true);
	m_selAnim = AnimValue(0.0, 5.0);
	m_submenuAnim = AnimValue(0.0, 3.0);
	populateMenu();
	if( m_first ) {
		std::string msg;
		if (!m_audio.hasPlayback()) msg = _("No playback devices could be used.\n");
		if (!msg.empty()) Game::getSingletonPtr()->dialog(msg + _("\nPlease configure some before playing."));
		m_first = false;
	}
	reloadGL();
	webserversetting = config["game/webserver_access"].i();
	m_audio.playSample("notice.ogg");
}

void ScreenIntro::reloadGL() {
	theme = std::make_unique<ThemeIntro>(showOpts);
	if (std::signbit(m_lineHeight) || std::signbit(m_lineSpacing)) {
		theme->option_selected->layout("");
		m_lineHeight = theme->option_selected->m_lineHeight;
		m_lineSpacing = theme->option_selected->m_lineSpacing();	
	}
}

void ScreenIntro::exit() {
	m_menu.clear();
	theme.reset();
}

void ScreenIntro::manageEvent(input::NavEvent const& event) {
	input::NavButton nav = event.button;
	if (nav == input::NAV_CANCEL) {
		if (m_menu.getSubmenuLevel() == 0) m_menu.moveToLast();  // Move cursor to quit in main menu
		else m_menu.closeSubmenu(); // One menu level up
	}
	else if (nav == input::NAV_DOWN || nav == input::NAV_MOREDOWN) m_menu.move(1);
	else if (nav == input::NAV_UP || nav == input::NAV_MOREUP) m_menu.move(-1);
	else if (nav == input::NAV_RIGHT && m_menu.getSubmenuLevel() >= 2) m_menu.action(1); // Config menu
	else if (nav == input::NAV_LEFT && m_menu.getSubmenuLevel() >= 2) m_menu.action(-1); // Config menu
	else if (nav == input::NAV_RIGHT && m_menu.getSubmenuLevel() < 2) m_menu.move(1); // Instrument nav hack
	else if (nav == input::NAV_LEFT && m_menu.getSubmenuLevel() < 2) m_menu.move(-1); // Instrument nav hack
	else if (nav == input::NAV_START) m_menu.action();
	else if (nav == input::NAV_PAUSE) m_audio.togglePause();
	// Animation targets
	std::clog << "event/debug: m_linesOnScreen: " << m_linesOnScreen << std::endl;
	std::clog << "menustack/debug: options: " << m_menu.getOptions().size() << ", curIndex: " << m_menu.curIndex() << std::endl;
	if (m_menu.curIndex() < m_menu.getOptions().size()) {
		m_selAnim.setTarget(m_menu.current().m_linePos);
	}
	std::clog << "selanim/debug: selanim:" << std::to_string(m_selAnim.getTarget()) << ", lineheight: " << theme->option_selected->m_lineHeight << std::endl;
	m_submenuAnim.setTarget(m_menu.getSubmenuLevel());
}

void ScreenIntro::manageEvent(SDL_Event event) {
	if (event.type == SDL_KEYDOWN && m_menu.getSubmenuLevel() > 0) {
		// These are only available in config menu
		int key = event.key.keysym.scancode;
		uint16_t modifier = event.key.keysym.mod;
		if (key == SDL_SCANCODE_R && modifier & Platform::shortcutModifier() && m_menu.current().value) {
			m_menu.current().value->reset(modifier & KMOD_ALT);
		}
		else if (key == SDL_SCANCODE_S && modifier & Platform::shortcutModifier()) {
			writeConfig(modifier & KMOD_ALT);
			Game::getSingletonPtr()->flashMessage((modifier & KMOD_ALT)
				? _("Settings saved as system defaults.") : _("Settings saved."));
		}
	}
}

void ScreenIntro::draw_menu_options() {
	// Variables used for positioning and other stuff
	float wcounter = 0;
	const float sel_margin = 0.03f;
	MenuOptions const& opts = m_menu.getOptions();
	float submenuanim = 1.0 - std::min(1.0, std::abs(m_submenuAnim.get()-m_menu.getSubmenuLevel()));
	theme->back_h.dimensions.fixedHeight(theme->option_selected->m_lineHeight);
	theme->back_h.dimensions.stretch(m_menu.dimensions.w(), theme->back_h.dimensions.h());
	// Determine from which item to start
	int start_i = std::min((int)m_menu.curIndex() - (m_menu.getSubmenuLevel() == 2 ? 0 : 1), (int)opts.size() - (int)showOpts
		+ (m_menu.getSubmenuLevel() == 2 ? 1 : 0)); // Hack to counter side-effects from displaying the value inside the menu
		std::clog << "start_i/debug: m_menu.curIndex(): " << m_menu.curIndex() << ", opts.size() - showOpts: " << ((int)opts.size() - (int)showOpts) << ", m_menu.getSubmenuLevel() == 2: " << std::to_string(m_menu.getSubmenuLevel() == 2) << std::endl;
	if (start_i < 0 || opts.size() == showOpts) start_i = 0;
	m_linesOnScreen = 0;
	// Loop the currently visible options
	for (size_t i = start_i, optTitleLines = 0, optValueLines = 0, itNewLines = 0; i < opts.size() && m_linesOnScreen < m_maxLinesOnScreen; ++i, m_linesOnScreen += itNewLines) {
		itNewLines = 0;
		float selanim = 0.0f;
// 		float extraSpacing = 0.0f;
		MenuOption const& opt = *opts[i].get();
		ColorTrans c(Color::alpha(submenuanim));
		std::shared_ptr<SvgTxtTheme> txt = getTextObject(opt.getName());
		std::shared_ptr<SvgTxtTheme> sel = theme->option_selected;
		std::clog << "menu/debug: i " << std::to_string(i) << ", start_i: " << std::to_string(start_i) << ", showOpts: " << showOpts << ", opts.size(): " << std::to_string(opts.size()) << std::endl;

		// Selection
		if (i == m_menu.curIndex()) {
			optTitleLines = 0;
			optValueLines = 0;
			if (opt.type != MenuOption::CHANGE_VALUE) {
				selanim = m_selAnim.get() - start_i;
				if (selanim < 0) selanim = 0;
			}
			if (i <= 1)
			std::clog << "position/debug: first item's center should be at m_start_y + " << std::to_string(m_lineHeight * m_linesOnScreen) << std::endl;
			// Animate selection higlight moving
			sel->dimensions().left(m_options_x).center(m_start_y + m_lineHeight * m_linesOnScreen);			/// Render text but don't draw it on screen yet.
			/// Render text but don't draw it on screen yet.
			sel->layout(opt.getName());
			std::clog << "intro/debug: option " << opt.getName() << ", m_linePos: " << std::to_string(opt.m_linePos) << ", m_linesOnScreen: " << std::to_string(m_linesOnScreen) << std::endl;
			sel->dimensions().left(m_options_x).center(m_start_y + m_lineHeight * m_linesOnScreen);
			if (opt.type == MenuOption::CHANGE_VALUE) {
				optTitleLines = sel->totalLines();
				m_selAnim.setTarget(m_lineHeight * (m_linesOnScreen + optTitleLines));
				selanim = m_selAnim.get() - start_i;
				if (selanim < 0) selanim = 0;
				txt->dimensions().left(m_options_x + sel_margin).center(m_start_y + m_lineHeight * (m_linesOnScreen + optTitleLines));
				txt->layout("< " + opt.value->getValue() + " >");
				txt->dimensions().left(m_options_x + sel_margin).center(m_start_y + m_lineHeight * (m_linesOnScreen + optTitleLines));
				optValueLines = txt->totalLines();
			}
			if ((m_linesOnScreen + optTitleLines + optValueLines) > m_maxLinesOnScreen) {
				std::clog << "menu/debug: max lines on screen: " << m_maxLinesOnScreen << ", lines already on screen: " << m_linesOnScreen << ", lines in current option (" << opt.getName() << "): " << sel->totalLines() << std::endl;
				break;
			} // If it's a value changer, the value will use _at least_ 1 more line.
			// Vertical-align highlight with selected option.
			theme->back_h.dimensions.left(m_options_x - sel_margin).center(m_start_y + selanim);
			theme->back_h.dimensions.fixedHeight(m_lineHeight * sel->totalLines());
			theme->back_h.draw();
			// Draw the text, dim if option not available
			{
				ColorTrans c(Color::alpha(opt.isActive() ? 1.0f : 0.5f));
			std::clog << "opt/debug: Will now draw " << opt.getName() << ", so far we're counting " << std::to_string(m_linesOnScreen) << " lines before it;" << " and it will add " << theme->option_selected->totalLines() << " more." << std::endl;
				sel->draw();
			}
			wcounter = std::max(wcounter, sel->w() + 2 * sel_margin); // Calculate the widest entry
			// If this is a config item, show the value below
			if (opt.type == MenuOption::CHANGE_VALUE) {
			std::clog << "optTitleLines/debug: rendering value for " << opt.getName() << "(" << opt.value->getValue() << "); optTitleLines is: " << optTitleLines << ", m_linesOnScreen is: " << m_linesOnScreen << ", value's totalLines: " << sel->totalLines() << std::endl;
			std::clog << "menu/debug: " << opt.getName() << ", y1: " << std::to_string(sel->dimensions().y1()) << ", yc: " << std::to_string(sel->dimensions().yc()) << ", height: " << std::to_string(sel->dimensions().h()) << std::endl;
				txt->draw();
// 				optTitleLines = sel->totalLines();
				itNewLines += (optTitleLines + optValueLines);
			}
			else {
				itNewLines += sel->totalLines();
			}
			if (std::signbit(opt.m_linePos)) {
// 				std::clog << "opt/debug: " << opt.getName() << ", address: " << std::addressof(opt) << std::endl;
// 				std::clog << "signpos/debug: " << opt.getName() << " has a negative m_linePos." << std::endl;
				opt.m_linePos = (m_lineHeight * m_linesOnScreen);
// 				std::clog << "back_h/debug: " << opt.getName() << ", m_linepos: " << std::to_string(opt.m_linePos) << ", m_lineHeight: " << std::to_string(m_lineHeight) << ", m_linesOnScreen: " << std::to_string(m_linesOnScreen) << ", m_lineSpacing: " << std::to_string(m_lineSpacing) << std::endl;
			}
		// Regular option (not selected)
		} else { 
			ColorTrans c(Color::alpha(opt.isActive() ? 1.0f : 0.5f));
			txt->dimensions().left(m_options_x).center(m_start_y + m_lineHeight * (m_linesOnScreen + (i > m_menu.curIndex() ? optValueLines : 0)));
			/// Render text but don't draw it on screen yet.
			txt->layout(opt.getName());
			std::clog << "optTitleLines/debug: rendering non-selected option (" << opt.getName() << "; optTitleLines is: " << optTitleLines << ", m_linesOnScreen is: " << m_linesOnScreen << ", txt's totalLines: " << txt->totalLines() << std::endl;
			if (m_linesOnScreen + txt->totalLines() + optTitleLines > m_maxLinesOnScreen) { 
				std::clog << "menu/debug: max lines on screen: " << m_maxLinesOnScreen << ", lines already on screen: " << m_linesOnScreen << ", lines in non-selected option (" << opt.getName() << "): " << txt->totalLines() << std::endl;
				break;
			}
			txt->dimensions().left(m_options_x).center(m_start_y + m_lineHeight * (m_linesOnScreen + (i > m_menu.curIndex() ? optValueLines : 0)));
			std::clog << "menu/debug: option: " << opt.getName() << ", y1: " << std::to_string(txt->dimensions().y1()) << ", yc: " << std::to_string(txt->dimensions().yc()) << ", height: " << std::to_string(txt->dimensions().h()) << std::endl;
			std::clog << "intro/debug: option " << opt.getName() << ", m_linePos: " << std::to_string(opt.m_linePos) << ", m_linesOnScreen: " << std::to_string(m_linesOnScreen) << ", signbit: " << std::to_string(std::signbit(opt.m_linePos)) << std::endl; 
			if (std::signbit(opt.m_linePos) || opt.type == MenuOption::CHANGE_VALUE) {
				std::clog << "signpos/debug: " << opt.getName() << " has a negative m_linePos, or is a value changer." << std::endl;
				opt.m_linePos = m_linesOnScreen * m_lineHeight;
				std::clog << "back_h/debug: " << opt.getName() << ", m_linepos: " << std::to_string(opt.m_linePos) << ", m_lineHeight: " << std::to_string(txt->m_lineHeight) << ", m_linesOnScreen: " << std::to_string(m_linesOnScreen) << ", m_lineSpacing: " << std::to_string(txt->m_lineSpacing()) << std::endl;
			}
			std::clog << "opt/debug: Will now draw " << opt.getName() << ", so far we're counting " << std::to_string(m_linesOnScreen) << " lines before it;" << " and it will add " << txt->totalLines() << " more." << std::endl;
			txt->draw();
			itNewLines += txt->totalLines();
			wcounter = std::max(wcounter, txt->w() + 2 * sel_margin); // Calculate the widest entry
		}
		std::clog << "loop/debug: reached end of iteration." << std::endl;
	}
	m_menu.dimensions.stretch(wcounter, theme->comment_bg.dimensions.y1() - 0.005f);
}

void ScreenIntro::draw() {
	glutil::GLErrorChecker glerror("ScreenIntro::draw()");
	{
		float anim = SDL_GetTicks() % 20000 / 20000.0;
		ColorTrans c(glmath::rotate(TAU * anim, glmath::vec3(1.0f, 1.0f, 1.0f)));
		theme->bg.draw();
	}
	if (m_menu.current().image) m_menu.current().image->draw();
	// Comment
	theme->comment->dimensions().left(-0.48f).screenBottom(-0.02f - theme->comment->totalLines() * theme->comment->m_lineHeight);
	theme->comment->layout(m_menu.current().getComment());
	theme->comment->dimensions().left(-0.48f).screenBottom(-0.02f - theme->comment->totalLines() * theme->comment->m_lineHeight);
	theme->comment_bg.dimensions.middle().center(theme->comment->dimensions().yc()).stretch(1.0f,theme->comment->m_lineHeight * theme->comment->totalLines() *1.03f);
	theme->comment_bg.draw();
	theme->comment->draw();
	// Calculate cut-off for text on screen; from where we begin drawing to a bit above the commment background.
	std::clog << "maxLines/debug: abs(m_start_y): " << std::to_string(std::abs(m_start_y)) << ", abs(theme->comment_bg.dimensions.y1()): " << std::to_string(std::abs(theme->comment_bg.dimensions.y1())) << std::endl;
	std::clog << "maxLines/debug: 0.5 - abs(m_start_y): " << std::to_string(0.5f - std::abs(m_start_y)) << ", 0.5 - abs(theme->comment_bg.dimensions.y1()): " << std::to_string(0.5f - std::abs(theme->comment_bg.dimensions.y1())) << std::endl;
	std::clog << "maxLines/debug: usable height as-per initial formula: " << std::to_string(0.96f - (std::abs(m_start_y) + std::abs(theme->comment_bg.dimensions.y1()))) << std::endl;
	std::clog << "maxLines/debug: usable height as-per new formula: " << std::to_string((0.5 - (std::abs(m_start_y) - m_lineHeight / 2)) + (0.5 - std::abs(theme->comment_bg.dimensions.y1()))) << std::endl;
	m_maxLinesOnScreen = ((0.96f - (std::abs(m_start_y) + std::abs(theme->comment_bg.dimensions.y1()))) / m_lineHeight - 1);
	// Key help for config
	if (m_menu.getSubmenuLevel() > 0) {
		theme->short_comment->dimensions().left(-0.48f).center(theme->comment_bg.dimensions.y1()-(0.01 + theme->short_comment->m_lineHeight /2));
		theme->short_comment->layout(_("Ctrl + S to save, Ctrl + R to reset defaults"));
theme->short_comment->dimensions().left(-0.48f).center(theme->comment_bg.dimensions.y1()-(0.01 + theme->short_comment->m_lineHeight /2));

		theme->short_comment_bg.dimensions.stretch(theme->short_comment->w() + 0.065f, 0.025f * theme->short_comment->totalLines());
		theme->short_comment_bg.dimensions.left(-0.54f).center(theme->short_comment->dimensions().yc());
		theme->short_comment_bg.draw();
		theme->short_comment->draw();
	}
	// Menu
	draw_menu_options();
// 	test_pos += 0.0000100f;
// theme->option_selected.dimensions.left(test_pos).center(theme->option_selected.dimensions.y1());
// 	theme->option_selected.draw(std::to_string(test_pos));
	draw_webserverNotice();
}

std::shared_ptr<SvgTxtTheme> ScreenIntro::getTextObject(std::string const& txt) {
	if (theme->options.find(txt) != theme->options.end()) return theme->options.at(txt);
	std::pair<std::string, std::shared_ptr<SvgTxtTheme>> kv = std::make_pair(txt, std::make_shared<SvgTxtTheme>(findFile("mainmenu_option.svg"), config["graphic/text_lod"].f(), WrappingStyle().menuScreenText(showOpts)));
	theme->options.insert(std::move(kv));
	return theme->options.at(txt);
}

void ScreenIntro::populateMenu() {
	MenuImage imgSing(new Texture(findFile("intro_sing.svg")));
	MenuImage imgPractice(new Texture(findFile("intro_practice.svg")));
	MenuImage imgConfig(new Texture(findFile("intro_configure.svg")));
	MenuImage imgQuit(new Texture(findFile("intro_quit.svg")));
	m_menu.clear();
	auto _perform = std::make_unique<MenuOption>(_("Perform"), _("Start performing!"), imgSing);
	_perform->screen("Songs");
	m_menu.add(std::move(_perform));
	auto _practice = std::make_unique<MenuOption>(_("Practice"), _("Check your skills or test the microphones"), imgPractice);
	_practice->screen("Practice");
	m_menu.add(std::move(_practice));
	// Configure menu + submenu options
	MenuOptions configmain;
	for (MenuEntry const& submenu: configMenu) {
		if (!submenu.items.empty()) {
			MenuOptions opts;
			// Process items that belong to that submenu
			for (std::string const& item: submenu.items) {
				ConfigItem& c = config[item];
				  std::unique_ptr<MenuOption> sm = std::make_unique<MenuOption>(_(c.getShortDesc().c_str()), _(c.getLongDesc().c_str()));
				  sm->changer(c);
				  opts.push_back(std::move(sm));
			}
// 			std::clog << "menu/debug: submenu has " << opts.size() << " items inside." << std::endl;
			std::unique_ptr<MenuOption> mo = std::make_unique<MenuOption>(_(submenu.shortDesc.c_str()), _(submenu.longDesc.c_str()), imgConfig);
			mo->submenu(std::move(opts));
			configmain.push_back(std::move(mo));
		} else {
			std::unique_ptr<MenuOption> mo = std::make_unique<MenuOption>(_(submenu.shortDesc.c_str()), _(submenu.longDesc.c_str()), imgConfig);
			mo->screen(submenu.name);
			configmain.push_back(std::move(mo));
		}
	}
// 	std::clog << "menu/debug: configmain vector size(): " << configmain.size() << std::endl;
	auto _configure = std::make_unique<MenuOption>(_("Configure"), _("Configure audio and game options"), imgConfig);
	_configure->submenu(std::move(configmain));
// 	std::clog << "menu/debug: configure options size: " << (*_configure).options->size() << std::endl;
	m_menu.add(std::move(_configure));
	auto _quit = std::make_unique<MenuOption>(_("Quit"), _("Leave the game"), imgQuit);
	_quit->screen("");
// 		std::clog << "menu/debug: adding Quit Item." << std::endl;
	m_menu.add(std::move(_quit));
}

#ifdef USE_WEBSERVER

void ScreenIntro::draw_webserverNotice() {
	if(m_webserverNoticeTimeout.get() == 0) {
		m_drawNotice = !m_drawNotice;
		m_webserverNoticeTimeout.setValue(5);
	}
	std::stringstream m_webserverStatusString;
	if((webserversetting == 1 || webserversetting == 2) && m_drawNotice) {
		std::string message = Game::getSingletonPtr()->subscribeWebserverMessages();		
		m_webserverStatusString << _("Webserver active!\n connect to this computer\nusing: ") << message;
		theme->WebserverNotice->draw(m_webserverStatusString.str());
	}
}

#else
void ScreenIntro::draw_webserverNotice() {}
#endif
