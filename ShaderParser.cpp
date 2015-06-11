#include "ShaderParser.h"

#include <SymbolTable.h>
#include <ParseHelper.h>
#include <Scan.h>
#include <ScanContext.h>
#include <InitializeDll.h>

using namespace glslang;

Shader Shader::parseSource(const std::string& code)
{
  Shader shader;
  const char* test[] = {"precision highp float;\n#line 1\n", code.c_str()};
  shader.setStrings(test, 2);

  TBuiltInResource resources;
  memset(&resources, 0, sizeof(resources));
  resources.limits.nonInductiveForLoops = true;

  shader.parse(&resources, 100, true, EShMsgDefault);
  //printf("%s\n", shader.getInfoLog());

  return shader;
}
