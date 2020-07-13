#include "configuration.hh"
#include "opengl_text.hh"

#include "libxml++-impl.hh"

#include "fontconfig/fontconfig.h"
#include <cmath>
#include <iostream>
#include <sstream>
#include "fs.hh"

void loadFonts() {
	FcConfig *config = FcInitLoadConfig();
	for (fs::path const& font: listFiles("fonts")) {
		FcBool err = FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(font.string().c_str()));
		std::clog << "font/info: Loading font " << font << ": " << ((err == FcTrue)?"ok":"error") << std::endl;
	}
	if (!FcConfigBuildFonts(config))
		throw std::logic_error("Could not build font database.");
	FcConfigSetCurrent(config);

	// This would all be very useless if pango+cairo didn't use the fontconfig+freetype backend:

	PangoCairoFontMap *map = PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default());
	std::clog << "font/info: PangoCairo is using font map " << G_OBJECT_TYPE_NAME(map) << std::endl;
	if (pango_cairo_font_map_get_font_type(map) != CAIRO_FONT_TYPE_FT) {
		PangoCairoFontMap *ftMap = PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_new_for_font_type(CAIRO_FONT_TYPE_FT));
		std::clog << "font/info: Switching to font map " << G_OBJECT_TYPE_NAME(ftMap) << std::endl;
		if (ftMap)
			pango_cairo_font_map_set_default(ftMap);
		else
			std::clog << "font/error: Can't switch to FreeType, fonts will be unavailable!" << std::endl;
	}
}

namespace {

}

WrappingStyle::WrappingStyle (float _maxWidth, EllipsizeMode _ellipsize, unsigned short int _maxLines) : m_maxLines(_maxLines * -1), m_ellipsize(_ellipsize), m_maxWidth((_maxWidth > 96.0f || std::signbit(_maxWidth)) ? 0.0f : _maxWidth) {};

OpenGLText::OpenGLText(std::shared_ptr<SvgTxtThemeBase> _owner) {
	float m = _owner->m_factor * 2.0f;  // HACK to improve text quality without affecting compatibility with old versions
	// Setup font settings
	std::clog << "opengltext/debug: Constructing" << std::endl;
	float border = _owner->m_text.stroke_width * m;
	int maxWidth = -1;
	PangoEllipsizeMode ellipsize;
	switch (_owner->m_wrapping.m_ellipsize) {
		case WrappingStyle::EllipsizeMode::NONE:
			ellipsize = (_owner->m_wrapping.m_maxLines == -1) ? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_MIDDLE;
			break;
		case WrappingStyle::EllipsizeMode::START:
			ellipsize = PANGO_ELLIPSIZE_START;
			break;
		case WrappingStyle::EllipsizeMode::MIDDLE:
			ellipsize = PANGO_ELLIPSIZE_MIDDLE;
			break;
		case WrappingStyle::EllipsizeMode::END:
			ellipsize = PANGO_ELLIPSIZE_END;
			break;
	}
	if (_owner->m_wrapping.m_maxWidth != 0.0f) { //FIXME: Got to adjust the maximum width according to the x_position.
		float screenEdge = static_cast<float>(targetWidth) * m * static_cast<float>(PANGO_SCALE);
	std::clog << "txt/debug: Text " << _owner->m_text.text << ", _owner->m_wrapping: maxLines(" << _owner->m_wrapping.m_maxLines << "), maxWidth(" << std::to_string(maxWidth) << "), ellipsize(" << std::to_string(ellipsize) << "), " << " current x position(" << _owner->dimensions().x1() << ")" << ", x anchor(" << _owner->dimensions().getXAnchor() << ")" << std::endl;

		float tempMaxWidth = _owner->m_wrapping.m_maxWidth / 100.0f;
		float posX = -0.5f - _owner->dimensions().x1();
		std::clog << "txt/debug: posX: " << posX << std::endl;
		tempMaxWidth += posX;
		std::clog << "txt/debug: tempMaxWidth: " << tempMaxWidth << std::endl;
		tempMaxWidth = std::min(tempMaxWidth, 0.96f);
		float wrapWidth = screenEdge * tempMaxWidth;
		maxWidth = static_cast<int>(std::round(wrapWidth));
		std::clog << "txt/debug: max width (" << std::to_string(wrapWidth) << ")" << std::endl;
	}
	pango_layout_set_ellipsize(_owner->m_pangoLayout.get(), ellipsize);
	pango_layout_set_width(_owner->m_pangoLayout.get(), maxWidth);
	if (_owner->m_wrapping.m_maxLines < 0) {
		pango_layout_set_height(_owner->m_pangoLayout.get(), _owner->m_wrapping.m_maxLines);
	}
	pango_layout_set_text(_owner->m_pangoLayout.get(), _owner->m_text.text.c_str(), -1);
	// Compute text extents
	{
		PangoRectangle rec;
		pango_layout_get_pixel_extents(_owner->m_pangoLayout.get(), nullptr, &rec);
		m_width = rec.width + border;  // Add twice half a border for margins
		m_height = rec.height + border;
	}
	m_lines = pango_layout_get_line_count (_owner->m_pangoLayout.get());
	if (m_lines > 0) {
		PangoRectangle lr;
		PangoLayoutLine* line = pango_layout_get_line_readonly(_owner->m_pangoLayout.get(), 0);
		pango_layout_line_get_pixel_extents(line, nullptr, &lr);
		float lh = lr.height;
		lh /= (m * targetHeight);
		_owner->m_lineHeight = lh;
	}
	for (unsigned i = 0; i < m_lines; i++) {
	PangoLayoutLine* line = pango_layout_get_line_readonly(_owner->m_pangoLayout.get(), i);
	int length = line->length;
	int start = line->start_index;
	std::string lineText(pango_layout_get_text(_owner->m_pangoLayout.get()),start,length);
	std::clog << "text/debug: (layout) Line " << std::to_string(i + 1) << ": \"" << lineText << "\", width is: " << std::to_string(m_width / m / targetWidth * 100) << "%, m_height: " << std::to_string(m_height / m / targetHeight * 100) << "%, did we wrap?: " << std::string(pango_layout_is_wrapped(_owner->m_pangoLayout.get()) ? "Yes" : "No") << std::endl;
// 	std::clog << "lineHeight/debug: line-height calculated by pango: " << std::to_string(lh) << std::endl;
	}
// 	float lineHeight;
// 	((m_height - (std::max(static_cast<size_t>(0), m_lines - 1) * _owner->m_text.linespacing) * m_height) / m_lines / m / targetHeight);
// 	std::clog << "lineHeight/debug: m_height: " << std::to_string(m_height) << ", m_lines: " << std::to_string(m_lines) << ", linespacing: " << _owner->m_text.linespacing << ", m: " << m << ", targetHeight: " << targetHeight << ", result: " << _owner->m_lineHeight << std::endl;
	// Create Cairo surface and drawing context
	std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> m_cairoSurface(
	  cairo_image_surface_create(CAIRO_FORMAT_ARGB32, m_width, m_height),
	  &cairo_surface_destroy);
	std::unique_ptr<cairo_t, decltype(&cairo_destroy)> m_cairoDc(
	  cairo_create(m_cairoSurface.get()),
	  &cairo_destroy);
	// Keep things sharp and fast, we scale with OpenGL anyway...
	cairo_set_antialias(m_cairoDc.get(), CAIRO_ANTIALIAS_FAST);
	cairo_push_group_with_content (m_cairoDc.get(), CAIRO_CONTENT_COLOR_ALPHA);
	cairo_set_operator(m_cairoDc.get(), CAIRO_OPERATOR_SOURCE);
	// Add Pango line and path to proper position on the DC
	cairo_move_to(m_cairoDc.get(), 0.5f * border, 0.5f * border);  // Margins needed for border stroke to fit in
	pango_cairo_update_layout(m_cairoDc.get(), _owner->m_pangoLayout.get());
	pango_cairo_layout_path(m_cairoDc.get(), _owner->m_pangoLayout.get());
	// Render text
	if (_owner->m_text.fill_col.a > 0.0f) {
		cairo_set_source_rgba(m_cairoDc.get(), _owner->m_text.fill_col.r, _owner->m_text.fill_col.g, _owner->m_text.fill_col.b, _owner->m_text.fill_col.a);
		cairo_fill_preserve(m_cairoDc.get());
	}
	// Render text border
	if (_owner->m_text.stroke_col.a > 0.0f) {
		// Use proper line-joins and caps.
		cairo_set_line_join (m_cairoDc.get(), _owner->m_text.LineJoin());
		cairo_set_line_cap (m_cairoDc.get(), _owner->m_text.LineCap());
		cairo_set_line_join (m_cairoDc.get(), _owner->m_text.LineJoin());
		cairo_set_miter_limit(m_cairoDc.get(), _owner->m_text.stroke_miterlimit);
		cairo_set_line_width(m_cairoDc.get(), border);
		cairo_set_source_rgba(m_cairoDc.get(), _owner->m_text.stroke_col.r, _owner->m_text.stroke_col.g, _owner->m_text.stroke_col.b, _owner->m_text.stroke_col.a);
		cairo_stroke(m_cairoDc.get());
	}
	cairo_pop_group_to_source (m_cairoDc.get());
	cairo_set_operator(m_cairoDc.get(),CAIRO_OPERATOR_OVER);
	cairo_paint (m_cairoDc.get());
	// Load into m_owner->m_texture (OpenGL texture)
	Bitmap bitmap(cairo_image_surface_get_data(m_cairoSurface.get()));
	bitmap.fmt = pix::INT_ARGB;
	bitmap.linearPremul = true;
	bitmap.resize(cairo_image_surface_get_width(m_cairoSurface.get()), cairo_image_surface_get_height(m_cairoSurface.get()));
	m_texture.load(bitmap, true);
	// We don't want text quality multiplier m to affect rendering size...
	m_width /= m;
	m_height /= m;
	std::clog << "texture/debug: x1: " << std::to_string(dimensions().x1()) << ", y1: " << std::to_string(dimensions().y1()) << ", width: " << std::to_string(dimensions().w()) << ", height: " << std::to_string(dimensions().h()) << std::endl;
// 	_owner->dimensions().stretch(m_width / targetWidth, m_height / targetHeight);
}

void OpenGLText::draw() {
	m_texture.draw();
}

void OpenGLText::draw(Dimensions &_dim, TexCoords &_tex) {
	m_texture.dimensions = _dim;
	m_texture.tex = _tex;
	m_texture.draw();
}

namespace {
	void parseTheme(fs::path const& themeFile, TextStyle &_theme, float &_width, float &_height, float &_x, float &_y, Align& _align) {
		xmlpp::Node::PrefixNsMap nsmap;
		nsmap["svg"] = "http://www.w3.org/2000/svg";
		nsmap["sodipodi"] = "http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd";
		xmlpp::DomParser dom(themeFile.string());
		// Parse width attribute
		auto n = dom.get_document()->get_root_node()->find("/svg:svg/@width",nsmap);
		if (n.empty()) throw std::runtime_error("Unable to find text theme info width in "+themeFile.string());
		xmlpp::Attribute& width = dynamic_cast<xmlpp::Attribute&>(*n[0]);
		_width = std::stof(width.get_value());
		// Parse height attribute
		n = dom.get_document()->get_root_node()->find("/svg:svg/@height",nsmap);
		if (n.empty()) throw std::runtime_error("Unable to find text theme info height in "+themeFile.string());
		xmlpp::Attribute& height = dynamic_cast<xmlpp::Attribute&>(*n[0]);
		_height = std::stof(height.get_value());
		// Parse text style attribute (CSS rules)
		n = dom.get_document()->get_root_node()->find("/svg:svg//svg:text/@sodipodi:linespacing",nsmap);
		if (n.empty()) { 
			_theme.linespacing = 0.0f;
		}
		else {	
			xmlpp::Attribute& linespacing = dynamic_cast<xmlpp::Attribute&>(*n[0]);
			float tmp = std::stof(linespacing.get_value());
			_theme.linespacing = ((tmp > 1.0f) ? tmp / 100.0f : tmp);
		}
		n = dom.get_document()->get_root_node()->find("/svg:svg//svg:text/@style",nsmap);
		if (n.empty()) throw std::runtime_error("Unable to find text theme info style in "+themeFile.string());
		xmlpp::Attribute& style = dynamic_cast<xmlpp::Attribute&>(*n[0]);
		std::istringstream iss(style.get_value());
		std::string token;
		while (std::getline(iss, token, ';')) {
			std::istringstream iss2(token);
			std::getline(iss2, token, ':');
			if (token == "font-size") {
				// Parse as int because https://llvm.org/bugs/show_bug.cgi?id=17782
				int value;
				iss2 >> value;
				_theme.fontsize = value;
			}
			else if (token == "font-family") std::getline(iss2, _theme.fontfamily);
			else if (token == "font-style") std::getline(iss2, _theme.fontstyle);
			else if (token == "font-weight") std::getline(iss2, _theme.fontweight);
			else if (token == "stroke-width") iss2 >> _theme.stroke_width;
			else if (token == "stroke-opacity") iss2 >> _theme.stroke_col.a;
			else if (token == "stroke-linejoin") iss2 >> _theme.stroke_linejoin;
			else if (token == "stroke-miterlimit") iss2 >> _theme.stroke_miterlimit;
			else if (token == "stroke-linecap") iss2 >> _theme.stroke_linecap;
			else if (token == "fill-opacity") iss2 >> _theme.fill_col.a;
			else if (token == "fill") iss2 >> _theme.fill_col;
			else if (token == "stroke") iss2 >> _theme.stroke_col;
			else if (token == "text-anchor") {
				std::string value;
				std::getline(iss2, value);
				_theme.fontalign = value;
				if (value == "start") _align = Align::LEFT;
				else if (value == "middle") _align = Align::CENTER;
				else if (value == "end") _align = Align::RIGHT;
			}
		}
		// Parse x and y attributes
		n = dom.get_document()->get_root_node()->find("/svg:svg//svg:text/@x",nsmap);
		if (n.empty()) throw std::runtime_error("Unable to find text theme info x in "+themeFile.string());
		xmlpp::Attribute& x = dynamic_cast<xmlpp::Attribute&>(*n[0]);
		_x = std::stof(x.get_value());
		n = dom.get_document()->get_root_node()->find("/svg:svg//svg:text/@y",nsmap);
		if (n.empty()) throw std::runtime_error("Unable to find text theme info y in "+themeFile.string());
		xmlpp::Attribute& y = dynamic_cast<xmlpp::Attribute&>(*n[0]);
		_y = std::stof(y.get_value());
	}
}

SvgTxtThemeBase::SvgTxtThemeBase(float factor, WrappingStyle _wrapping) :
	m_factor(factor),
	m_wrapping(_wrapping),
	m_pangoContext(std::unique_ptr<PangoContext, decltype(&g_object_unref)>(
	  pango_font_map_create_context(pango_cairo_font_map_get_default()),
	  &g_object_unref)),
	m_pangoLayout(std::unique_ptr<PangoLayout, decltype(&g_object_unref)>(
	  pango_layout_new(m_pangoContext.get()),
	  &g_object_unref)) {
	  	std::clog << "Constructing SvgTxtThemeBase with factor: " << factor << std::endl;
	}
	
void SvgTxtThemeBase::applyTheme() {
	PangoAlignment alignment = m_text.parseAlignment();
	pango_layout_set_wrap(m_pangoLayout.get(), PANGO_WRAP_WORD_CHAR);
	pango_font_description_set_weight(m_text.fontDesc.get(), m_text.parseWeight());
	pango_font_description_set_style(m_text.fontDesc.get(), m_text.parseStyle());
	pango_font_description_set_family(m_text.fontDesc.get(), m_text.fontfamily.c_str());
	pango_font_description_set_absolute_size(m_text.fontDesc.get(), m_text.fontsize * PANGO_SCALE * (m_factor * 2.0f));
	pango_layout_set_single_paragraph_mode(m_pangoLayout.get(), false);
	pango_layout_set_alignment(m_pangoLayout.get(), alignment);
	float spacing = pango_font_description_get_size(m_text.fontDesc.get());
	std::clog << "theme/debug: Default line-spacing: " << std::to_string(spacing) << std::endl;
	spacing *= m_text.linespacing;
	std::clog << "theme/debug: Modified line-spacing: " << std::to_string(spacing) << std::endl;
	pango_layout_set_spacing(m_pangoLayout.get(),spacing);
	std::clog << "theme/debug: Modified line-spacing applied: " << std::to_string(pango_layout_get_spacing(m_pangoLayout.get())) << std::endl;
	pango_layout_set_font_description(m_pangoLayout.get(), m_text.fontDesc.get());
	std::clog << "Applied theme to text, factor is: " << m_factor << std::endl;
}

SvgTxtThemeSimple::SvgTxtThemeSimple(fs::path const& themeFile, float factor, WrappingStyle wrapping) : SvgTxtThemeBase(factor, wrapping) {
	Align a;
	float tmp;
	parseTheme(themeFile, m_text, tmp, tmp, tmp, tmp, a);
	applyTheme();
	std::clog << "svgtxtthemesimple/debug: Constructed new instance." << std::endl;
}

void SvgTxtThemeSimple::render(std::string _text) {
	if (!m_opengl_text.get() || m_cache_text != _text) {
		m_cache_text = _text;
		m_text.text = _text;
		Dimensions dim;
		m_opengl_text = std::make_unique<OpenGLText>(shared_from_this());
	}
}

void SvgTxtThemeSimple::draw() {
	m_opengl_text->draw();
}

SvgTxtTheme::SvgTxtTheme(fs::path const& themeFile, float factor, WrappingStyle wrapping): SvgTxtThemeBase(factor, wrapping), m_align() {
	parseTheme(themeFile, m_text, m_width, m_height, m_x, m_y, m_align);
	std::clog << "theme/debug: read x: " << std::to_string(m_x) << ", y: " << std::to_string(m_y) << ", width: " << std::to_string(m_width) << ", height: " << std::to_string(m_height) << std::endl;
	applyTheme();
	std::clog << "text/debug: Constructing SvgTxtTheme; will call dimensions.middle() with argument (" << std::to_string((-0.5f + m_x / m_width)) << ") and center() with argument (" << std::to_string((m_y - 0.5f * m_height) / m_width) << ")" << std::endl;
	m_dimensions.stretch(0.0f,0.0f).middle(-0.5f + m_x / m_width).center((m_y - 0.5 * m_height) / m_width);
	std::clog << "svgtxttheme/debug: Constructed new instance." << std::endl;
	std::clog << "pango2/debug: Creating new instance of PangoContext and PangoLayout" << std::endl;
}

void SvgTxtTheme::setHighlight(fs::path const& themeFile) {
	float a,b,c,d;
	Align e;
	parseTheme(themeFile, m_text_highlight, a, b, c, d, e);
	applyTheme();
}

void SvgTxtTheme::layout(std::string const& _text) {
	layout(_text, m_factor);
}

void SvgTxtTheme::layout(std::string const& _text, float factor) {
	std::clog << "txt/debug: called layout() with std::string text: " << _text << ", and factor: " << factor << std::endl;
	std::vector<TZoomText> tmp;
	TZoomText t(_text);
	t.factor = (factor / config["graphic/text_lod"].f());
	tmp.push_back(t);
	layout(tmp);
}

void SvgTxtTheme::layout(std::vector<TZoomText> const& _text, bool padSyllables) {
	std::string tmp;
	for (TZoomText const& zt: _text) {
// 		std::clog << "layout1/debug: zt.string: " << zt.string << ", zt.factor: " << std::to_string(zt.factor) << std::endl;
		tmp += zt.string;
	}
	if (m_opengl_text.size() != _text.size() || m_cache_text != tmp) {
		m_cache_text = tmp;
		m_opengl_text.clear();
		for (TZoomText const& zt: _text) {
			m_text.text = zt.string;
			m_opengl_text.push_back(
			  std::make_unique<OpenGLText>(shared_from_this())
			);
			m_opengl_text.back()->m_factor = zt.factor;
		}
	}
	else if (padSyllables) {
		for (size_t i = 0; i < m_opengl_text.size() && i < _text.size(); ++i) {
			m_opengl_text[i]->m_factor = _text[i].factor;		
		}
	}
	if (padSyllables) draw(true);
}

size_t SvgTxtTheme::totalLines() {
	size_t lines = 1;
	for (std::unique_ptr<OpenGLText> const& text: m_opengl_text) {
		size_t current_lines = text->lines();
		lines = (current_lines > lines ? current_lines : lines);
	}
	return lines;
}

void SvgTxtTheme::draw(std::string const& text, bool padSyllables) {
	std::clog << "txt/debug: called draw() with std::string text: " << text << std::endl;
	layout(text);
	draw(padSyllables);
}

void SvgTxtTheme::draw(bool padSyllables) {
	float text_w = 0.0f;
	float text_h = 0.0f;
	// First compute maximum height and whole length
	for (std::unique_ptr<OpenGLText> const& _text: m_opengl_text) {
		text_w += _text->w();
		std::clog << "text/debug: text has " << totalLines() << " lines." << std::endl;
		if (m_multiLine()) {
			std::clog << "text/debug: text has been wrapped." << std::endl;
			text_h += _text->h();
		} 
		else {
			text_h = std::max(text_h, _text->h());
		}
		std::clog << "text/debug: text_w: " << _text->w() << ", text_h: " << text_h << std::endl;
	}

	float texture_ar = text_w / text_h;
	m_texture_width = std::min(padSyllables ? 0.864f : 0.96f, text_w / targetWidth); // targetWidth is defined in video_driver.cc, it's the base rendering width, used to project the svg onto a gltexture. currently we're targeting 1366x768 as base resolution.
	
	float position_x = m_dimensions.x1();
	std::clog << "text/debug: Initial position_x inside SvgTxtTheme::draw is: " << std::to_string(position_x) << std::endl;
	if (m_align == Align::CENTER) position_x -= 0.5f * (m_texture_width * (padSyllables ? 1.1f : 1.0f));
	if (m_align == Align::RIGHT) position_x -= m_texture_width;

	if ((position_x + m_texture_width * (padSyllables ? 1.1f : 1.0f)) > (padSyllables ? 0.432f : 0.48f)) { 
		m_texture_width = ((padSyllables ? 0.432f : 0.48f) - position_x / (padSyllables ? 1.1f : 1.0f)) ;
	}
	m_texture_height = m_texture_width / texture_ar; // Keep aspect ratio.
	for (std::unique_ptr<OpenGLText> const& _text: m_opengl_text) {
		float syllable_x = _text->w();
		float syllable_width = syllable_x *  m_texture_width / text_w * _text->m_factor;
		float syllable_height = m_texture_height * _text->m_factor;
		float syllable_ar = syllable_width / syllable_height;
		Dimensions dim(syllable_ar);
		dim.fixedHeight(m_texture_height).center(m_dimensions.yc());
		dim.middle(position_x + 0.5f * dim.w());
		TexCoords tex;
		std::clog << "text/debug: Text (inside draw()): \"" << m_cache_text << "\", position_x: " << position_x << ", texture_width: " << m_texture_width << ", texture_ar: " << texture_ar << ",texture_height: " << m_texture_height << std::endl;
		if (_text->m_factor > 1.0f) {
			LyricColorTrans lc(m_text.fill_col, m_text.stroke_col, m_text_highlight.fill_col, m_text_highlight.stroke_col);
			dim.fixedWidth(dim.w() * _text->m_factor);
			_text->draw(dim, tex);
		} 
		else { _text->draw(dim, tex); }
		position_x += (syllable_width / _text->m_factor) * (padSyllables ? 1.1f : 1.0f);
	}
}

