#pragma once


#include "color.hh"
#include "glmath.hh"
#include <GL/glew.h>
#include <string>
#include <iostream>
#include <vector>

namespace glutil {

	// Note: if you reorder or otherwise change the contents of this, VertexArray::Draw() must be modified accordingly
	struct VertexInfo {
		glmath::vec3 position;
		glmath::vec2 texCoord;
		glmath::vec3 normal;
		glmath::vec4 color;
		VertexInfo():
		  position(0.0, 0.0, 0.0),
		  texCoord(0.0, 0.0),
		  normal(0.0, 0.0, 0.0),
		  color(1.0, 1.0, 1.0, 1.0)
		{}
	};

	/// Handy vertex array capable of drawing itself
	class VertexArray {
	  private:
		std::vector<VertexInfo> m_vertices;
		VertexInfo m_vert;
	  public:
		VertexArray() {}

		VertexArray& Vertex(float x, float y, float z = 0.0f) {
			return Vertex(glmath::vec3(x, y, z));
		}

		VertexArray& Vertex(glmath::vec3 const& v) {
			m_vert.position = v;
			m_vertices.push_back(m_vert);
			m_vert = VertexInfo();
			return *this;
		}

		VertexArray& Normal(float x, float y, float z) {
			return Normal(glmath::vec3(x, y, z));
		}

		VertexArray& Normal(glmath::vec3 const& v) {
			m_vert.normal = v;
			return *this;
		}

		VertexArray& TexCoord(float s, float t) {
			return TexCoord(glmath::vec2(s, t));
		}

		VertexArray& TexCoord(glmath::vec2 const& v) {
			m_vert.texCoord = v;
			return *this;
		}

		VertexArray& Color(glmath::vec4 const& v) {
			m_vert.color = v;
			return *this;
		}

		void Draw(GLint mode = GL_TRIANGLE_STRIP);

		bool empty() const {
			return m_vertices.empty();
		}

		unsigned size() const {
			return m_vertices.size();
		}

		void clear() {
			m_vertices.clear();
		}

	};

	/// Wrapper struct for RAII
	struct UseDepthTest {
		/// enable depth test (for 3d objects)
		UseDepthTest() {
			glClear(GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
		}
		~UseDepthTest() {
			glDisable(GL_DEPTH_TEST);
		}
	};

	/// Checks for OpenGL error and displays it with given location info
	struct GLErrorChecker {
		std::string info;
		GLErrorChecker(std::string const& info): info(info) { check("precondition"); }
		~GLErrorChecker() { check("postcondition"); }
		void check(std::string const& what = "check()") {
			GLenum err = glGetError();
			if (err == GL_NO_ERROR) return;
			std::clog << "opengl/error: " << msg(err) << " in " << info << " " << what << std::endl;
		}
		static void reset() { glGetError(); }
		static std::string msg(GLenum err) {
			switch(err) {
				case GL_NO_ERROR: return std::string();
				case GL_INVALID_ENUM: return "Invalid enum";
				case GL_INVALID_VALUE: return "Invalid value";
				case GL_INVALID_OPERATION: return "Invalid operation";
				case GL_INVALID_FRAMEBUFFER_OPERATION: return "FBO is not complete";
				case GL_STACK_OVERFLOW: return "Stack overflow";
				case GL_STACK_UNDERFLOW: return "Stack underflow";
				case GL_OUT_OF_MEMORY: return "Out of memory";
				default: return "Unknown error";
			}
		}
	};
}


