#ifndef __SHADER_PARSER_H__
#define __SHADER_PARSER_H__

#include <ParseHelper.h>

class Shader : public glslang::TShader {
public:
  Shader() : TShader(EShLangFragment) {
  }
  glslang::TIntermediate* getIntermediate() {
    return intermediate;
  }

  static Shader parseSource(const std::string& code);
};

#endif // __SHADER_PARSER_H__
