#include <windows.h>

#include <GL/glew.h>

#include <GL/freeglut.h>

#include <GL/gl.h>
#include <GL/glext.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

#include "InitShader.h"
#include "imgui_impl_glut.h"
#include "VideoMux.h"
#include "DebugCallback.h"

//names of the shader files to load
static const std::string vertex_shader("tfb_vs.glsl");
static const std::string fragment_shader("tfb_fs.glsl");

GLuint shader_program = -1;

//Ping-pong pairs of objects and buffers since we don't have read/write access to VBOs.
GLuint vao[2] = {-1, -1};
GLuint vbo[2] = {-1, -1};
const int num_particles = 10000;
GLuint tfo[2] = { -1, -1 }; //transform feedback objects

//These indices get swapped every frame to perform the ping-ponging
int Read_Index = 0;  //initially read from VBO_ID[0]
int Write_Index = 1; //initially write to VBO_ID[1]


float time_sec = 0.0f;
float angle = 0.0f;
bool recording = false;

//Draw the user interface using ImGui
void draw_gui()
{
   ImGui_ImplGlut_NewFrame();

   const int filename_len = 256;
   static char video_filename[filename_len] = "capture.mp4";

   ImGui::InputText("Video filename", video_filename, filename_len);
   ImGui::SameLine();
   if (recording == false)
   {
      if (ImGui::Button("Start Recording"))
      {
         const int w = glutGet(GLUT_WINDOW_WIDTH);
         const int h = glutGet(GLUT_WINDOW_HEIGHT);
         recording = true;
         start_encoding(video_filename, w, h); //Uses ffmpeg
      }
      
   }
   else
   {
      if (ImGui::Button("Stop Recording"))
      {
         recording = false;
         finish_encoding(); //Uses ffmpeg
      }
   }

   //create a slider to change the angle variables
   ImGui::SliderFloat("View angle", &angle, -3.141592f, +3.141592f);

   static float slider = 0.5f;
   if(ImGui::SliderFloat("Slider", &slider, 0.0f, 1.0f))
   {
      glUniform1f(2, slider);
   }

   //static bool show = false;
   //ImGui::ShowTestWindow(&show);
   ImGui::Render();
 }

// glut display callback function.
// This function gets called every time the scene gets redisplayed 
void display()
{
   //clear the screen
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glUseProgram(shader_program);

   const int w = glutGet(GLUT_WINDOW_WIDTH);
   const int h = glutGet(GLUT_WINDOW_HEIGHT);
   const float aspect_ratio = float(w) / float(h);

   glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f));
   glm::mat4 V = glm::lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
   glm::mat4 P = glm::perspective(3.141592f / 4.0f, aspect_ratio, 0.1f, 100.0f);

   int PVM_loc = glGetUniformLocation(shader_program, "PVM");
   if (PVM_loc != -1)
   {
      glm::mat4 PVM = P*V*M;
      glUniformMatrix4fv(PVM_loc, 1, false, glm::value_ptr(PVM));
   }

   const GLint pos_loc = 0;
   const GLint vel_loc = 1;
   const GLint age_loc = 2;

   //Bind the current write transform feedback object.
   glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfo[Write_Index]);

   //Binding the transform feedback object recalls the buffer range state shown below. In fact, if
   //your system does not support transform feedback objects you can uncomment the following lines.
   //glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, pos_loc, vbo[Write_Index], 0, num_particles * 3 * sizeof(float));
   //glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, vel_loc, vbo[Write_Index], num_particles * 3 * sizeof(float), num_particles * 3 * sizeof(float));
   //glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, age_loc, vbo[Write_Index], num_particles * 6 * sizeof(float), num_particles * 1 * sizeof(float));


   glDepthMask(GL_FALSE);
   glBindVertexArray(vao[Read_Index]);
   //Prepare the pipeline for transform feedback
   glBeginTransformFeedback(GL_POINTS);
   glDrawArrays(GL_POINTS, 0, num_particles);
   glEndTransformFeedback();

   glDepthMask(GL_TRUE);


         
   if (recording == true)
   {
      glFinish();

      glReadBuffer(GL_BACK);
      read_frame_to_encode(&rgb, &pixels, w, h);
      encode_frame(rgb);
   }

   draw_gui();

   glutSwapBuffers();

   //Ping-pong the buffers.
   Read_Index = 1 - Read_Index;
   Write_Index = 1 - Write_Index;
}

// glut idle callback.
//This function gets called between frames
void idle()
{
	glutPostRedisplay();

   const int time_ms = glutGet(GLUT_ELAPSED_TIME);
   time_sec = 0.001f*time_ms;

   glUniform1f(1, time_sec);
}

void reload_shader()
{
   GLuint new_shader = InitShader(vertex_shader.c_str(), fragment_shader.c_str());

   if(new_shader == -1) // loading failed
   {
      glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
   }
   else
   {
      glClearColor(0.01f, 0.01f, 0.01f, 0.0f);

      if(shader_program != -1)
      {
         glDeleteProgram(shader_program);
      }
      shader_program = new_shader;
      
      //You need to specify which varying variables will capture transform feedback values.
      const char *vars[] = { "pos_out", "vel_out", "age_out" };
      glTransformFeedbackVaryings(shader_program, 3, vars, GL_SEPARATE_ATTRIBS);

      //Must relink the program after specifying transform feedback varyings.
      glLinkProgram(shader_program);
      int status;
      glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
      if (status == GL_FALSE)
      {
         printProgramLinkError(shader_program);
      }
   }
}

// Display info about the OpenGL implementation provided by the graphics driver.
// Your version should be > 4.0 for CGT 521 
void printGlInfo()
{
   std::cout << "Vendor: "       << glGetString(GL_VENDOR)                    << std::endl;
   std::cout << "Renderer: "     << glGetString(GL_RENDERER)                  << std::endl;
   std::cout << "Version: "      << glGetString(GL_VERSION)                   << std::endl;
   std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)  << std::endl;
}

#define BUFFER_OFFSET( offset )   ((GLvoid*) (offset))

void initOpenGl()
{
   //Initialize glew so that new OpenGL function names can be used
   glewInit();

   //RegisterCallback();

   glEnable(GL_DEPTH_TEST);

   //Enable alpha blending
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE); //additive alpha blending

   //Allow setting point size in fragment shader
   glEnable(GL_PROGRAM_POINT_SIZE);

   //reload shader is modified to specify transform feedback varyings
   reload_shader();
   
   //create TFOs
   glGenTransformFeedbacks(2, tfo);
  
   float zeros[7*num_particles] = {0.0f}; 
   //create VAOs and VBOs
   glGenVertexArrays(2, vao);
   glGenBuffers(2, vbo);   

   //These are the attribute locations we specify in the shader.
   const GLint pos_loc = 0;
   const GLint vel_loc = 1;
   const GLint age_loc = 2;

   //Init Write Buffers.
   glBindVertexArray(vao[Write_Index]); 
   glBindBuffer(GL_ARRAY_BUFFER, vbo[Write_Index]); 
   glBufferData(GL_ARRAY_BUFFER, sizeof(zeros), zeros, GL_DYNAMIC_COPY);

   glEnableVertexAttribArray(pos_loc); //enable this attribute
   glVertexAttribPointer(pos_loc, 3, GL_FLOAT, false, 0, BUFFER_OFFSET(0)); //tell opengl how to get the attribute values out of the vbo (stride and offset)

   glEnableVertexAttribArray(vel_loc); //enable this attribute
   glVertexAttribPointer(vel_loc, 3, GL_FLOAT, false, 0, BUFFER_OFFSET(num_particles * sizeof(glm::vec3))); //tell opengl how to get the attribute values out of the vbo (stride and offset)
                                                                                                      
   glEnableVertexAttribArray(age_loc); //enable this attribute
   glVertexAttribPointer(age_loc, 1, GL_FLOAT, false, 0, BUFFER_OFFSET(2 * num_particles * sizeof(glm::vec3))); //tell opengl how to get the attribute values out of the vbo (stride and offset)

   //Tell the TFO where each varying variable should be written in the VBO.
   glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfo[Write_Index]);
   glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, pos_loc, vbo[Write_Index], 0, num_particles * 3 * sizeof(float));
   glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, vel_loc, vbo[Write_Index], num_particles * 3 * sizeof(float), num_particles * 3 * sizeof(float));
   glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, age_loc, vbo[Write_Index], num_particles * 6 * sizeof(float), num_particles * 1 * sizeof(float));
   glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

   //Init Read Buffers.
   glBindVertexArray(vao[Read_Index]); 
   glBindBuffer(GL_ARRAY_BUFFER, vbo[Read_Index]);
   glBufferData(GL_ARRAY_BUFFER, sizeof(zeros), zeros, GL_DYNAMIC_COPY);

   glEnableVertexAttribArray(pos_loc); 
   glVertexAttribPointer(pos_loc, 3, GL_FLOAT, false, 0, BUFFER_OFFSET(0)); //tell opengl how to get the attribute values out of the vbo (stride and offset)

   glEnableVertexAttribArray(vel_loc); 
   glVertexAttribPointer(vel_loc, 3, GL_FLOAT, false, 0, BUFFER_OFFSET(num_particles*sizeof(glm::vec3))); //tell opengl how to get the attribute values out of the vbo (stride and offset)

   glEnableVertexAttribArray(age_loc); 
   glVertexAttribPointer(age_loc, 1, GL_FLOAT, false, 0, BUFFER_OFFSET(2*num_particles * sizeof(glm::vec3))); //tell opengl how to get the attribute values out of the vbo (stride and offset) 

   //Tell the TFO where each varying variable should be written in the VBO.
   glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfo[Read_Index]);
   glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, pos_loc, vbo[Read_Index], 0, num_particles * 3 * sizeof(float));
   glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, vel_loc, vbo[Read_Index], num_particles * 3 * sizeof(float), num_particles * 3 * sizeof(float));
   glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, age_loc, vbo[Read_Index], num_particles * 6 * sizeof(float), num_particles * 1 * sizeof(float));
   glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0); //unbind transform feedback

   glBindVertexArray(0); //unbind vao
   glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind vbo


}

// glut callbacks need to send keyboard and mouse events to imgui
void keyboard(unsigned char key, int x, int y)
{
   ImGui_ImplGlut_KeyCallback(key);
   std::cout << "key : " << key << ", x: " << x << ", y: " << y << std::endl;

   switch(key)
   {
      case 'r':
      case 'R':
         reload_shader();     
      break;
   }
}

void keyboard_up(unsigned char key, int x, int y)
{
   ImGui_ImplGlut_KeyUpCallback(key);
}

void special_up(int key, int x, int y)
{
   ImGui_ImplGlut_SpecialUpCallback(key);
}

void passive(int x, int y)
{
   ImGui_ImplGlut_PassiveMouseMotionCallback(x,y);
}

void special(int key, int x, int y)
{
   ImGui_ImplGlut_SpecialCallback(key);
}

void motion(int x, int y)
{
   ImGui_ImplGlut_MouseMotionCallback(x, y);
}

void mouse(int button, int state, int x, int y)
{
   ImGui_ImplGlut_MouseButtonCallback(button, state);
}


int main (int argc, char **argv)
{
   //Configure initial window state using freeglut

#if _DEBUG
   glutInitContextFlags(GLUT_DEBUG);
#endif
   glutInitContextVersion(4, 3);

   glutInit(&argc, argv); 
   glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
   glutInitWindowPosition (5, 5);
   glutInitWindowSize (1920, 1080);
   int win = glutCreateWindow ("Transform Feedback Demo");

   printGlInfo();

   //Register callback functions with glut. 
   glutDisplayFunc(display); 
   glutKeyboardFunc(keyboard);
   glutSpecialFunc(special);
   glutKeyboardUpFunc(keyboard_up);
   glutSpecialUpFunc(special_up);
   glutMouseFunc(mouse);
   glutMotionFunc(motion);
   glutPassiveMotionFunc(motion);
   glutIdleFunc(idle);

   initOpenGl();
   ImGui_ImplGlut_Init(); // initialize the imgui system

   //Enter the glut event loop.
   glutMainLoop();
   glutDestroyWindow(win);
   return 0;		
}


