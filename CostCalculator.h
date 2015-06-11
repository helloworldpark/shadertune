#ifndef __COST_CALCULATOR_H__
#define __COST_CALCULATOR_H__

#include <string>
#include "ShaderParser.h"

class CostCalculator {
public:
  CostCalculator(const std::string& code);
  bool shaderError() const {
    return shaderError_;
  }
  const std::string& shaderErrorString() const {
    return shaderErrorString_;
  }
  const std::map<int,int>& getCosts() const {
    return costs_;
  }
private:
  std::map<int,int> costs_;
  std::string shaderErrorString_;
  bool shaderError_;
};

#endif // __COST_CALCULATOR_H__
