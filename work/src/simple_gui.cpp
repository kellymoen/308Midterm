//---------------------------------------------------------------------------
//
// Copyright (c) 2016 Taehyun Rhee, Joshua Scott, Ben Allen
//
// This software is provided 'as-is' for assignment of COMP308 in ECS,
// Victoria University of Wellington, without any express or implied warranty. 
// In no event will the authors be held liable for any damages arising from
// the use of this software.
//
// The contents of this file may not be copied or duplicated in any form
// without the prior permission of its owner.
//
//----------------------------------------------------------------------------

#include "simple_gui.hpp"

#include <iostream>

using namespace std;

namespace cgra {

	namespace {

		// Data
		static GLFWwindow*  g_window = NULL;
		static double       g_time = 0.0f;
		static bool         g_mousePressed[3] = { false, false, false };
		static float        g_mouseWheel = 0.0f;
		static GLuint       g_fontTexture = 0;
		static int          g_shaderHandle = 0, g_vertHandle = 0, g_fragHandle = 0;
		static int          g_attribLocationTex = 0, g_attribLocationProjMtx = 0;
		static int          g_attribLocationPosition = 0, g_attribLocationUV = 0, g_attribLocationColor = 0;
		static unsigned int g_vboHandle = 0, g_vaoHandle = 0, g_elementsHandle = 0;

		static void createFontsTexture() {
			ImGuiIO& io = ImGui::GetIO();

			unsigned char* pixels;
			int width, height;
			io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits for OpenGL3 demo because it is more likely to be compatible with user's existing shader.

			glGenTextures(1, &g_fontTexture);
			glBindTexture(GL_TEXTURE_2D, g_fontTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

			// Store our identifier
			io.Fonts->TexID = (void *)(intptr_t)g_fontTexture;

			// Cleanup (don't clear the input data if you want to append new fonts later)
			io.Fonts->ClearInputData();
			io.Fonts->ClearTexData();
		}

		static bool createDeviceObjects() {
			// Backup GL state
			GLint last_texture, last_array_buffer, last_vertex_array;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
			glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
			glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

			const GLchar *vertex_shader =
				"#version 130\n"
				"uniform mat4 ProjMtx;\n"
				"in vec2 Position;\n"
				"in vec2 UV;\n"
				"in vec4 Color;\n"
				"out vec2 Frag_UV;\n"
				"out vec4 Frag_Color;\n"
				"void main()\n"
				"{\n"
				"   Frag_UV = UV;\n"
				"   Frag_Color = Color;\n"
				"   gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
				"}\n";

			const GLchar* fragment_shader =
				"#version 130\n"
				"uniform sampler2D Texture;\n"
				"in vec2 Frag_UV;\n"
				"in vec4 Frag_Color;\n"
				"out vec4 Out_Color;\n"
				"void main()\n"
				"{\n"
				"   Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
				"}\n";

			g_shaderHandle = glCreateProgram();
			g_vertHandle = glCreateShader(GL_VERTEX_SHADER);
			g_fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(g_vertHandle, 1, &vertex_shader, 0);
			glShaderSource(g_fragHandle, 1, &fragment_shader, 0);
			glCompileShader(g_vertHandle);
			glCompileShader(g_fragHandle);
			glAttachShader(g_shaderHandle, g_vertHandle);
			glAttachShader(g_shaderHandle, g_fragHandle);
			glLinkProgram(g_shaderHandle);

			g_attribLocationTex = glGetUniformLocation(g_shaderHandle, "Texture");
			g_attribLocationProjMtx = glGetUniformLocation(g_shaderHandle, "ProjMtx");
			g_attribLocationPosition = glGetAttribLocation(g_shaderHandle, "Position");
			g_attribLocationUV = glGetAttribLocation(g_shaderHandle, "UV");
			g_attribLocationColor = glGetAttribLocation(g_shaderHandle, "Color");

			glGenBuffers(1, &g_vboHandle);
			glGenBuffers(1, &g_elementsHandle);

			glGenVertexArrays(1, &g_vaoHandle);
			glBindVertexArray(g_vaoHandle);
			glBindBuffer(GL_ARRAY_BUFFER, g_vboHandle);
			glEnableVertexAttribArray(g_attribLocationPosition);
			glEnableVertexAttribArray(g_attribLocationUV);
			glEnableVertexAttribArray(g_attribLocationColor);

		#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
			glVertexAttribPointer(g_attribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
			glVertexAttribPointer(g_attribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
			glVertexAttribPointer(g_attribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
		#undef OFFSETOF

			createFontsTexture();

			// Restore modified GL state
			glBindTexture(GL_TEXTURE_2D, last_texture);
			glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
			glBindVertexArray(last_vertex_array);

			return true;
		}

		static void renderDrawLists(ImDrawData* draw_data) {
			// Backup GL state
			GLint last_program, last_texture, last_array_buffer, last_element_array_buffer, last_vertex_array;
			glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
			glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
			glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
			glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

			// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_SCISSOR_TEST);
			glActiveTexture(GL_TEXTURE0);

			// Handle cases of screen coordinates != from framebuffer coordinates (e.g. retina displays)
			ImGuiIO& io = ImGui::GetIO();
			float fb_height = io.DisplaySize.y * io.DisplayFramebufferScale.y;
			draw_data->ScaleClipRects(io.DisplayFramebufferScale);

			// Setup orthographic projection matrix
			const float ortho_projection[4][4] =
			{
				{ 2.0f/io.DisplaySize.x, 0.0f,                   0.0f, 0.0f },
				{ 0.0f,                  2.0f/-io.DisplaySize.y, 0.0f, 0.0f },
				{ 0.0f,                  0.0f,                  -1.0f, 0.0f },
				{-1.0f,                  1.0f,                   0.0f, 1.0f },
			};
			glUseProgram(g_shaderHandle);
			glUniform1i(g_attribLocationTex, 0);
			glUniformMatrix4fv(g_attribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
			glBindVertexArray(g_vaoHandle);

			for (int n = 0; n < draw_data->CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = draw_data->CmdLists[n];
				const ImDrawIdx* idx_buffer_offset = 0;

				glBindBuffer(GL_ARRAY_BUFFER, g_vboHandle);
				glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.size() * sizeof(ImDrawVert), (GLvoid*)&cmd_list->VtxBuffer.front(), GL_STREAM_DRAW);

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_elementsHandle);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx), (GLvoid*)&cmd_list->IdxBuffer.front(), GL_STREAM_DRAW);

				for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++)
				{
					if (pcmd->UserCallback)
					{
						pcmd->UserCallback(cmd_list, pcmd);
					}
					else
					{
						glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
						glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
						glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, GL_UNSIGNED_SHORT, idx_buffer_offset);
					}
					idx_buffer_offset += pcmd->ElemCount;
				}
			}

			// Restore modified GL state
			glUseProgram(last_program);
			glBindTexture(GL_TEXTURE_2D, last_texture);
			glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
			glBindVertexArray(last_vertex_array);
			glDisable(GL_SCISSOR_TEST);

			glDisable(GL_BLEND);
		}


		static const char* getClipboardText() {
			return glfwGetClipboardString(g_window);
		}


		static void setClipboardText(const char* text) {
			glfwSetClipboardString(g_window, text);
		}

	}

	namespace SimpleGUI {

		void mouseButtonCallback(GLFWwindow*, int button, int action, int /*mods*/) {
			if (action == GLFW_PRESS && button >= 0 && button < 3)
				g_mousePressed[button] = true;
		}


		void scrollCallback(GLFWwindow*, double /*xoffset*/, double yoffset) {
			g_mouseWheel += (float)yoffset; // Use fractional mouse wheel, 1.0 unit 5 lines.
		}


		void keyCallback(GLFWwindow*, int key, int /*scancode*/, int action, int mods) {
			ImGuiIO& io = ImGui::GetIO();
			if (action == GLFW_PRESS)
				io.KeysDown[key] = true;
			if (action == GLFW_RELEASE)
				io.KeysDown[key] = false;

			(void)mods; // Modifiers are not reliable across systems
			io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
			io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
			io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
		}

		void charCallback(GLFWwindow*, unsigned int c) {
			ImGuiIO& io = ImGui::GetIO();
			if (c > 0 && c < 0x10000)
				io.AddInputCharacter((unsigned short)c);
		}


		bool init(GLFWwindow* window, bool install_callbacks) {
			g_window = window;

			ImGuiIO& io = ImGui::GetIO();
			// Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
			io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
			io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
			io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
			io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
			io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
			io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
			io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
			io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
			io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
			io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
			io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
			io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
			io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
			io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
			io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
			io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
			io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
			io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
			io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

			// Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
			io.RenderDrawListsFn = renderDrawLists;
			io.SetClipboardTextFn = setClipboardText;
			io.GetClipboardTextFn = getClipboardText;
		#ifdef _WIN32
			io.ImeWindowHandle = glfwGetWin32Window(g_window);
		#endif
			if (install_callbacks) {
				glfwSetMouseButtonCallback(window, mouseButtonCallback);
				glfwSetScrollCallback(window, scrollCallback);
				glfwSetKeyCallback(window, keyCallback);
				glfwSetCharCallback(window, charCallback);
			}

			return true;
		}


		void shutdown() {
			if (g_vaoHandle) glDeleteVertexArrays(1, &g_vaoHandle);
			if (g_vboHandle) glDeleteBuffers(1, &g_vboHandle);
			if (g_elementsHandle) glDeleteBuffers(1, &g_elementsHandle);
			g_vaoHandle = g_vboHandle = g_elementsHandle = 0;

			glDetachShader(g_shaderHandle, g_vertHandle);
			glDeleteShader(g_vertHandle);
			g_vertHandle = 0;

			glDetachShader(g_shaderHandle, g_fragHandle);
			glDeleteShader(g_fragHandle);
			g_fragHandle = 0;

			glDeleteProgram(g_shaderHandle);
			g_shaderHandle = 0;

			if (g_fontTexture)
			{
				glDeleteTextures(1, &g_fontTexture);
				ImGui::GetIO().Fonts->TexID = 0;
				g_fontTexture = 0;
			}
			ImGui::Shutdown();
		}


		void newFrame() {
			if (!g_fontTexture)
				createDeviceObjects();

			ImGuiIO& io = ImGui::GetIO();

			// Setup display size (every frame to accommodate for window resizing)
			int w, h;
			int display_w, display_h;
			glfwGetWindowSize(g_window, &w, &h);
			glfwGetFramebufferSize(g_window, &display_w, &display_h);
			io.DisplaySize = ImVec2((float)w, (float)h);
			io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);

			// Setup time step
			double current_time =  glfwGetTime();
			io.DeltaTime = g_time > 0.0 ? (float)(current_time - g_time) : (float)(1.0f/60.0f);
			g_time = current_time;

			// Setup inputs
			// (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
			if (glfwGetWindowAttrib(g_window, GLFW_FOCUSED)) {
				double mouse_x, mouse_y;
				glfwGetCursorPos(g_window, &mouse_x, &mouse_y);
				io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);   // Mouse position in screen coordinates (set to -1,-1 if no mouse / on another screen, etc.)
			} else {
				io.MousePos = ImVec2(-1,-1);
			}

			for (int i = 0; i < 3; i++) {
				io.MouseDown[i] = g_mousePressed[i] || glfwGetMouseButton(g_window, i) != 0;    // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
				g_mousePressed[i] = false;
			}

			io.MouseWheel = g_mouseWheel;
			g_mouseWheel = 0.0f;

			// Hide OS mouse cursor if ImGui is drawing it
			glfwSetInputMode(g_window, GLFW_CURSOR, io.MouseDrawCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);

			// Start the frame
			ImGui::NewFrame();
		}

		void render() {
			ImGui::Render();
		}
	}

}