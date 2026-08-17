//
// Created by ryan on 5/26/24.
//
#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Rendering {
 class ShaderObject;

 class ShaderProgram
 {
 public:

  //explicit needed to avoid unnecessary implicit conversions
  explicit ShaderProgram(const std::string& FileName);
  ShaderProgram& operator= (ShaderProgram const& sp) = default;

  GLuint GetProgramID() const;

  /** AttachShaderObject
   * @param Object - the shader object to attach to the shader program
   */
  void AttachShaderObject(const std::shared_ptr<ShaderObject> &Object);

  /** CompileAttachedShaders - attempts to compile all attached shader objects
   * @return true if all shader objects compiled successfully
   */
  bool CompileAttachedShaders();

  /** LinkShaderProgram
   * @return true if linking the shader program was successful
   */
  bool LinkShaderProgram();

  /** ReloadShaderObjects
   * ReloadShaderObjects all shader objects
   */
  void ReloadShaderObjects();

 protected:
  //name of the shader program
  std::string ProgramName;

  //opengl shader program identifier
  GLuint ProgramID;

  //attached shader objects
  std::vector<std::shared_ptr<ShaderObject>> AttachedShaderObjects;
 };
}
