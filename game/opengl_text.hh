#pragma once

#include "color.hh"
#include "texture.hh"
#include "unicode.hh"
#include <pango/pangocairo.h>
#include <memory>
#include <vector>

class SvgTxtThemeBase;

/// horizontal align
enum class Align {A_ASIS, LEFT, CENTER, RIGHT};

/// Load custom fonts from current theme and data folders
void loadFonts();

/// zoomed text
struct TZoomText {
	/// text
	std::string string;
	/// zoomfactor
	float factor;
	/// constructor
	TZoomText(std::string const& str = std::string()): string(str), factor(1.0f) {}
};

/// Special theme for creating opengl themed surfaces
/** this structure does not include:
 *  - library specific field
 *  - global positionning
 * the font{family,style,weight,align} are the one found into SVGs
 */
struct TextStyle {
	cairo_line_join_t LineJoin() {	///< convert svg string to cairo enum.
		if (UnicodeUtil::toLower(stroke_linejoin) == "round") return CAIRO_LINE_JOIN_ROUND;
		if (UnicodeUtil::toLower(stroke_linejoin) == "bevel") return CAIRO_LINE_JOIN_BEVEL;	
		return CAIRO_LINE_JOIN_MITER;
	};
	cairo_line_cap_t LineCap() {	///< convert svg string to cairo enum.
		if (UnicodeUtil::toLower(stroke_linecap) == "round") return CAIRO_LINE_CAP_ROUND;
		if (UnicodeUtil::toLower(stroke_linecap) == "square") return CAIRO_LINE_CAP_SQUARE;	
		return CAIRO_LINE_CAP_BUTT;
	};
	PangoAlignment parseAlignment() {
		if (fontalign == "start") return PANGO_ALIGN_LEFT;
		if (fontalign == "center" || fontalign == "middle") return PANGO_ALIGN_CENTER;
		if (fontalign == "end") return PANGO_ALIGN_RIGHT;
		throw std::logic_error(fontalign + ": Unknown font alignment (opengl_text.hh)");
	};
	PangoStyle parseStyle() {
		if (fontstyle == "normal") return PANGO_STYLE_NORMAL;
		if (fontstyle == "italic") return PANGO_STYLE_ITALIC;
		if (fontstyle == "oblique") return PANGO_STYLE_OBLIQUE;
		throw std::logic_error(fontstyle + ": Unknown font style (opengl_text.hh)");
	};
	PangoWeight parseWeight() {
		if (fontweight == "normal") return PANGO_WEIGHT_NORMAL;
		if (fontweight == "bold") return PANGO_WEIGHT_BOLD;
		if (fontweight == "bolder") return PANGO_WEIGHT_ULTRABOLD;
		throw std::logic_error(fontweight + ": Unknown font weight (opengl_text.hh)");
	};
	Color fill_col; ///< fill color
	Color stroke_col; ///< stroke color
	float stroke_width; ///< stroke thickness
	float stroke_miterlimit; ///< stroke miter limit
	float fontsize; ///< fontsize
	float linespacing; ///< line spacing
	std::string fontfamily; ///< fontfamily
	std::string fontstyle; ///< fontstyle
	std::string fontweight; ///< fontweight
	std::string fontalign; ///< alignment
	std::string stroke_linejoin; ///< stroke line-join type
	std::string stroke_linecap; ///< stroke line-join type
	std::string text; ///< text
	std::unique_ptr<PangoFontDescription, decltype(&pango_font_description_free)> fontDesc;
	TextStyle(): stroke_width(),
		stroke_miterlimit(1.0f),
		fontsize(),
		fontDesc(std::unique_ptr<PangoFontDescription, decltype(&pango_font_description_free)>(
		    pango_font_description_new(),
		    &pango_font_description_free)) {
		std::clog << "textstyle/debug: Constructed new instance." << std::endl;
		std::clog << "pango1/debug: Creating new instance of PangoFontDescription" << std::endl;
	}
};
	
/// Convenience container for deciding how a given OpenGLText instance will be wrapped, ellipsized or fitted to the display area.
struct WrappingStyle {
	enum class EllipsizeMode { NONE, START, MIDDLE, END };

	/// constructor
	WrappingStyle(float _maxWidth = 0.0f, EllipsizeMode _ellipsize = EllipsizeMode::NONE, unsigned short int _maxLines = 1);
	
	/// setters
	WrappingStyle& ellipsizeNone(unsigned short int lines = 1) { m_maxLines = (lines * -1); m_ellipsize = EllipsizeMode::NONE; return *this; }
	WrappingStyle& ellipsizeStart(unsigned short int lines = 1) { m_maxLines = (lines * -1); m_ellipsize = EllipsizeMode::START; return *this; }
	WrappingStyle& ellipsizeMiddle(unsigned short int lines = 1) { m_maxLines = (lines * -1); m_ellipsize = EllipsizeMode::MIDDLE; return *this; }
	WrappingStyle& ellipsizeEnd(unsigned short int lines = 1) { m_maxLines = (lines * -1); m_ellipsize = EllipsizeMode::END; return *this; }
	WrappingStyle& setWidth(float _maxWidth = 0.0f) { m_maxWidth = (_maxWidth > 96.0f || std::signbit(_maxWidth)) ? 0.0f : _maxWidth; return *this; }
	
	/// presets
	WrappingStyle& menuScreenText(unsigned short int lines = 0) { setWidth(96.0f); ellipsizeMiddle(lines); return *this;  } ///< No line limit, wrap at screen edge and/or ellipsize middle.
	WrappingStyle& lyrics() { ellipsizeNone(1); setWidth(96.0f); return *this;  } ///< Default one line, wrap at screen edge, don't ellipsize or wrap.
	
	/** maximum lines.
	  * This is an int because Pango is a mess.
	  * Values (for Pango):
	  *   <=-1: Maximum amount of lines per paragraph.
	  *      0: Maximum one line in the entire layout.
	  *    >=1: Maximum height in PANGO_UNITS.
	  * We'll always be using negative values, and we'll use the "0" to signal we should not set a line limit at all.
	*/
	short int m_maxLines;
	/// ellipsis style
	EllipsizeMode m_ellipsize;
	/// maximum width, valid values are 0 to 100 (percent of screen).
	/// 0 means no limit, and in practice it will turn off wrapping of text.
	/// 96 (being used as the default), in practice means wrap or ellipsize at the edge of the screen minus at least a 2% margin on each side.
	unsigned short int m_maxWidth;
};

/// this class will enable to create a texture from a themed text structure
/** it will not cache any data (class using this class should)
 * it provides size of the texture are drawn (x,y)
 * it provides size of the texture created (x_power_of_two, y_power_of_two)
 */
class OpenGLText {
public:
	/// constructor
	OpenGLText(std::shared_ptr<SvgTxtThemeBase> _owner);
	/// draws area
	void draw(Dimensions &_dim, TexCoords &_tex);
	/// draws full texture
	void draw();
	/// @return width
	float w() const { return m_width; }
	/// @return height
	float h() const { return m_height; }
	/// @returns dimension of texture
	Dimensions& dimensions() { return m_texture.dimensions; }
	/// @return number of lines rendered.
	size_t lines() { return m_lines; }
	/// @return multiplication factor.
	float m_factor = 1.0f;

private:
	size_t m_lines = 1;
	float m_width;
	float m_height;
	Texture m_texture;
};

class SvgTxtThemeBase : public std::enable_shared_from_this<SvgTxtThemeBase> {
	public:
	friend class OpenGLText;
	float m_lineHeight;
	protected:
	std::string m_cache_text;
	TextStyle m_text;
	float m_factor;
	Dimensions m_dimensions;
	WrappingStyle m_wrapping;
	std::unique_ptr<PangoContext, decltype(&g_object_unref)> m_pangoContext;
	std::unique_ptr<PangoLayout, decltype(&g_object_unref)> m_pangoLayout;
	virtual size_t totalLines() = 0;
	bool m_multiLine() { return totalLines() > 1; }
 	virtual Dimensions& dimensions() = 0;
 	void applyTheme();
 	public:
	/// constructor
	SvgTxtThemeBase(float factor = 1.0f, WrappingStyle _wrapping = WrappingStyle().menuScreenText());
	/// destructor
	virtual ~SvgTxtThemeBase() {};
};

/// themed svg texts (simple)
class SvgTxtThemeSimple : public SvgTxtThemeBase {
public:
	/// constructor
	SvgTxtThemeSimple(fs::path const& themeFile, float factor = 1.0f, WrappingStyle _wrapping = WrappingStyle().menuScreenText());
	/// renders text
	void render(std::string _text);
	/// draws texture
	void draw();
	/// gets dimensions
	Dimensions& dimensions() { 
		if (m_opengl_text) return m_opengl_text->dimensions();
		return m_dimensions;
		}
	/// Returns the number of lines in a contained OpenGLText.
	size_t totalLines() { return m_opengl_text->lines(); }

private:
	std::unique_ptr<OpenGLText> m_opengl_text;
};

/// themed svg texts
class SvgTxtTheme : public SvgTxtThemeBase{
public:
	friend class OpenGLText;
	/// enum declaration Gravity:
	/** <pre>
	 * +----+---+----+
	 * | NW | N | NE |
	 * +----+---+----+
	 * | W  | C |  E |
	 * +----+---+----+
	 * | SW | S | SE |
	 * +----+---+----+ (and ASIS)
	 *
	 * Coord:
	 * (x1,y1)        (x2,y1)
	 *    +--------------+
	 *    |              |
	 *    |              |
	 *    +--------------+
	 * (x1,y2)        (x2,y2)
	 *
	 * gravity will affect how fit-inside/fit-outside will work
	 * fitting will always keep aspect ratio
	 * fit-inside: will fit inside if texture is too big
	 * force-fit-inside: will always stretch to fill at least one of the axis
	 * fit-outside: will fit outside if texture is too small
	 * force-fit-outside: will always stretch to fill both axis
	 * gravity does not mean position, it is only an anchor
	 * Fixed points:
	 *   NW: (x1,y1)
	 *   N:  ((x1+x2)/2,y1)
	 *   NE: (x2,y1)
	 *   W:  (x1,(y1+y2)/2)
	 *   C:  ((x1+x2)/2,(y1+y2)/2)
	 *   E:  (x2,(y1+y2)/2)
	 *   SW: (x1,y2)
	 *   S:  ((x1+x2)/2,y2)
	 *   SE: (x2,y2)</pre>
	 */
	/// TODO anchors
	enum Gravity {NW,N,NE,W,C,E,SW,S,SE};
	/// where to position when space is too small
	enum Fitting {F_ASIS, INSIDE, OUTSIDE, FORCE_INSIDE, FORCE_OUTSIDE};
	/// vertical align
	enum VAlign {V_ASIS, TOP, MIDDLE, BOTTOM};
	/// dimensions, what else
	Dimensions& dimensions() { return m_dimensions; }
	/// constructor
	SvgTxtTheme(fs::path const& themeFile, float factor = 1.0f, WrappingStyle _wrapping = WrappingStyle().menuScreenText());
	/// layout text
	void layout(std::string const& _text);
	void layout(std::string const& _text, float factor);
	void layout(std::vector<TZoomText> const& _text, bool padSyllables = false);
	/// draws text with alpha
	void draw(std::string const& text, bool padSyllables = false);
	void draw(bool padSyllables = false);
	/// render text with alpha
// 	void render();
	/// sets highlight
	void setHighlight(fs::path const& themeFile);
	/// width
	float w() const { return m_texture_width; }
	/// height
	float h() const { return m_texture_height; }
	/// set align
	void setAlign(Align align) { m_align = align; }
	/// Returns the maximum number of lines in a contained OpenGLText.
	size_t totalLines();
	float m_lineSpacing() { return m_text.linespacing; }
	
private:
	Align m_align;
	std::vector<std::unique_ptr<OpenGLText>> m_opengl_text;
	float m_x;
	float m_y;
	float m_width;
	float m_height;
	float m_texture_width;
	float m_texture_height;
	TextStyle m_text_highlight;
};

