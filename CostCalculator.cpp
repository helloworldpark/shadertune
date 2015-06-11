#include "CostCalculator.h"

using namespace glslang;

class TraverseBase : public TIntermTraverser {
public:
  TraverseBase(bool preVisit = true, bool inVisit = false, bool postVisit = false, bool rightToLeft = false) : TIntermTraverser(preVisit, inVisit, postVisit, rightToLeft) {
  }
  size_t calcLoopIterations(TIntermLoop* node) {
    TIntermTyped* loopTest = node->getTest();
    if (!loopTest) {
      return 0;
    }
    TIntermBinary* binop = loopTest->getAsBinaryNode();
    if (binop->getOp() != EOpLessThan) {
      return 0;
    }
    TIntermConstantUnion* constant = binop->getRight()->getAsConstantUnion();
    if (!constant) {
      return 0;
    }
    const TConstUnionArray& array = constant->getConstArray();
    const TConstUnion& value = array[0];
    if (value.getType() != EbtInt) {
      return 0;
    }
    return value.getIConst();
  }
  bool nodeShouldCount(TIntermNode* node) {
    if (node->getAsOperator()) {
      TIntermOperator* op = node->getAsOperator();
      switch (op->getOp()) {
      case EOpFunction:
      case EOpSequence:
      case EOpParameters:
      case EOpLinkerObjects:
      case EOpAssign:
      case EOpVectorSwizzle:
      /* logical operators */
      case EOpEqual:
      case EOpNotEqual:
      case EOpVectorEqual:
      case EOpVectorNotEqual:
      case EOpLessThan:
      case EOpGreaterThan:
      case EOpLessThanEqual:
      case EOpGreaterThanEqual:
      case EOpComma:
      case EOpLogicalOr:
      case EOpLogicalXor:
      case EOpLogicalAnd:
        return false;
      default:
        return true;
      }
    }
    return true;
  }
  size_t getTypeSize(TIntermTyped* node) {
    if (!node) {
      printf("Error, null node!\n");
      return 0;
    }
    if (node->isVector()) {
      return node->getVectorSize();
    } else if (node->isMatrix()) {
      return node->getMatrixCols() * node->getMatrixRows();
    } else if (node->isScalar()) {
      return 1;
    } else {
      return 0;
    }
  }
  size_t getTypeElementSize(TIntermTyped* node) {
    if (!node) {
      printf("Error, null node!\n");
      return 0;
    }
    if (node->isVector()) {
      return 1;
    } else if (node->isMatrix()) {
      return node->getMatrixCols();
    } else if (node->isScalar()) {
      return 1;
    } else {
      return 0;
    }
  }
};

class OperationCostTraverser : public TraverseBase {
public:
  OperationCostTraverser() : TraverseBase(true, false, true) {
  }
  const std::map<TIntermNode*, size_t>& getCosts() {
    return costs_;
  }
  virtual bool visitAggregate(TVisit visit, TIntermAggregate* node) {
    if (visit == EvPreVisit && nodeShouldCount(node)) {
      if (node->getOp() != EOpFunctionCall) {
        costs_[node] = getTypeSize(node);
      }
    }
    return true;
  }
  virtual bool visitBinary(TVisit visit, TIntermBinary* node) {
    if (visit == EvPostVisit && nodeShouldCount(node)) {
      TIntermNode* left = node->getLeft();
      TIntermNode* right = node->getRight();
      if (node->getOp() == EOpVectorSwizzle && right->getAsAggregate()) {
        const TIntermSequence& seq = right->getAsAggregate()->getSequence();
        costs_[node] = size_t(seq.size());
      } else if (node->getOp() == EOpIndexDirect) {
        costs_[node] = getTypeElementSize(left->getAsTyped());
      } else {
        costs_[node] = std::max(costs_[node->getLeft()], costs_[node->getRight()]);
      }
    }
    return true;
  }
  virtual bool visitUnary(TVisit visit, TIntermUnary* node) {
    if (visit == EvPostVisit && nodeShouldCount(node)) {
      costs_[node] = costs_[node->getOperand()];
    }
    return true;
  }
  virtual void visitSymbol(TIntermSymbol* node) {
    costs_[node] = getTypeSize(node);
  }
  std::map<TIntermNode*, size_t> costs_;
};

class LoopCostTraverser : public TraverseBase {
public:
  LoopCostTraverser() : TraverseBase(true, false, true), loopCount_(0) {
  }
  const std::map<TIntermNode*, size_t>& getCosts() {
    return costs_;
  }
  virtual bool visitAggregate(TVisit visit, TIntermAggregate* node) {
    if (visit == EvPostVisit) {
      costs_[node] = (!loopCount_ ? 1 : loopCount_);
    }
    return true;
  }
  virtual bool visitBinary(TVisit visit, TIntermBinary* node) {
    if (visit == EvPostVisit) {
      costs_[node] = (!loopCount_ ? 1 : loopCount_);
    }
    return true;
  }
  virtual bool visitUnary(TVisit visit, TIntermUnary* node) {
    if (visit == EvPostVisit) {
      costs_[node] = (!loopCount_ ? 1 : loopCount_);
    }
    return true;
  }
  virtual bool visitLoop(TVisit visit, TIntermLoop* node) {
    if (visit == EvPreVisit) {
      loopCount_ += calcLoopIterations(node);
    }
    if (visit == EvPostVisit) {
      loopCount_ -= calcLoopIterations(node);
    }
    return true;
  }
  size_t loopCount_;
  std::map<TIntermNode*, size_t> costs_;
};

class FunctionCostTraverser : public TraverseBase {
public:
  struct CallSite {
    CallSite(const std::string& to, TIntermNode* from) : to(to), from(from) {}
    std::string to;
    TIntermNode* from;
  };
  FunctionCostTraverser(const std::map<TIntermNode*, size_t>& loopCosts) : TraverseBase(true, false, true), loopCosts_(loopCosts) {
  }
  const std::map<TIntermNode*, std::string>& functionMap() const {
    return functions_;
  }
  const std::map<std::string, std::vector<CallSite> >& callMap() const {
    return calls_;
  }
  std::string signature(TIntermAggregate* node) {
    if (!node) {
      return "(no_function)";
    }
    TString r = node->getName();
    return r.c_str();
  }
  TIntermAggregate* myFunction() const {
    for (size_t i = 0; i < path.size(); ++i) {
      if (path[i]->getAsOperator()) {
        TIntermOperator* op = path[i]->getAsOperator();
        if (op->getOp() == EOpFunction) {
          return path[i]->getAsAggregate();
        }
      }
    }
    return 0;
  }
  virtual bool visitAggregate(TVisit visit, TIntermAggregate* node) {
    if (visit == EvPostVisit) {
      std::string mysig = signature(myFunction());
      if (node->getOp() == EOpFunctionCall) {
        std::string sig = signature(node);
        calls_[mysig].push_back(CallSite(sig, node));
      }
      functions_[node] = mysig;
    }
    return true;
  }
  virtual bool visitBinary(TVisit visit, TIntermBinary* node) {
    if (visit == EvPostVisit) {
      functions_[node] = signature(myFunction());
    }
    return true;
  }
  virtual bool visitUnary(TVisit visit, TIntermUnary* node) {
    if (visit == EvPostVisit) {
      functions_[node] = signature(myFunction());
    }
    return true;
  }
  std::map<TIntermNode*, size_t> loopCosts_; /* number of iterations of a particular node */
  std::map<TIntermNode*, std::string> functions_; /* which function is a node in? */
  std::map<std::string, std::vector<CallSite> > calls_; /* who calls who? */
};

class CostSumTraverser : public TraverseBase {
public:
  CostSumTraverser(const std::map<TIntermNode*, size_t>& sizes) : TraverseBase(true, false, true), sizes_(sizes) {
  }
  const std::map<int,int>& getCosts() const {
    return costs_;
  }
  void updateCost(TIntermNode* node, const size_t cost) {
    TSourceLoc loc = node->getLoc();
    if (costs_.find(loc.line) == costs_.end()) {
      costs_[loc.line] = 0;
    }
    costs_[loc.line] += cost;
  }
  virtual bool visitAggregate(TVisit visit, TIntermAggregate* node) {
    if (visit == EvPostVisit && nodeShouldCount(node)) {
      if (node->getOp() == EOpFunctionCall) {
        updateCost(node, sizes_[node]);
      } else {
        const TIntermSequence& seq = node->getSequence();
        size_t cost = 0;
        if (seq.size() && seq[0]->getAsTyped()) {
          cost = getTypeSize(seq[0]->getAsTyped());
        } else {
          cost = 0;
        }
        updateCost(node, cost);
      }
    }
    return true;
  }
  virtual bool visitBinary(TVisit visit, TIntermBinary* node) {
    if (visit == EvPostVisit && nodeShouldCount(node)) {
      updateCost(node, sizes_[node]);
    }
    return true;
  }
  virtual bool visitUnary(TVisit visit, TIntermUnary* node) {
    if (visit == EvPostVisit && nodeShouldCount(node)) {
      updateCost(node, sizes_[node]);
    }
    return true;
  }
private:
  std::map<TIntermNode*, size_t> sizes_;
  std::map<int,int> costs_;
};

std::string getCallSignature(TIntermNode* node) {
  /* returns null if not a call site */
  if (node->getAsAggregate()) {
    TIntermAggregate* ag = node->getAsAggregate();
    if (ag->getOp() == EOpFunctionCall) {
      TString sig = ag->getName();
      return sig.c_str();
    }
  }
  return std::string();
}

std::map<TIntermNode*, size_t> calcOperationCosts(OperationCostTraverser& oct, LoopCostTraverser& lct, FunctionCostTraverser& fct) {
  std::map<TIntermNode*, size_t> operationCosts = oct.getCosts();

  std::map<TIntermNode*, size_t> loopCosts = lct.getCosts();

  std::map<TIntermNode*, std::string> functionMap = fct.functionMap();
  std::map<std::string, std::vector<FunctionCostTraverser::CallSite> > callMap = fct.callMap();

  /* update opreation costs with loop costs */
  {
    std::map<TIntermNode*, size_t>::iterator i = operationCosts.begin();
    for ( ; i != operationCosts.end(); ++i) {
      i->second *= loopCosts[i->first];
    }
  }

  /* compute raw cost of each function as a simple sum of its operation counts */
  std::map<std::string, bool> needResolve;
  std::map<std::string, size_t> functionCosts;
  std::map<std::string, size_t> invocationCount;
  {
    std::map<TIntermNode*, size_t>::const_iterator i = operationCosts.begin();
    for ( ; i != operationCosts.end(); ++i) {
      functionCosts[functionMap[i->first]] += i->second;
      /* all functions are initially resolved */
      needResolve[functionMap[i->first]] = false;
    }
  }

  /* mark dependencies as needing resolve */
  {
    std::map<std::string, std::vector<FunctionCostTraverser::CallSite> >::const_iterator i = callMap.begin();
    for ( ; i != callMap.end(); ++i) {
      needResolve[i->first] = true;
    }
  }

  /* initialize link order with call graph leaf nodes */
  std::vector<std::string> linkOrder;
  {
    std::map<std::string, bool> linked;
    std::map<std::string, std::vector<FunctionCostTraverser::CallSite> >::const_iterator i = callMap.begin();
    for ( ; i != callMap.end(); ++i) {
      for (size_t j = 0; j < i->second.size(); ++j) {
        const std::string sig = i->second[j].to;
        if (!linked[sig]) {
          linkOrder.push_back(sig);
          linked[sig] = true;
        }
      }
    }
  }

  /* resolve any functions that now have resolved dependencies */
  bool didResolve = true;
  while (didResolve) {
    didResolve = false;
    std::map<std::string, std::vector<FunctionCostTraverser::CallSite> >::const_iterator i = callMap.begin();
    for ( ; i != callMap.end(); ++i) {
      if (needResolve[i->first]) {
        /* check its dependencies to see if they are all resolved */
        bool resolved = true;
        for (size_t j = 0; j < i->second.size(); ++j) {
          if (needResolve[i->second[j].to]) {
            resolved = false;
            break;
          }
        }
        if (resolved) { /* all dependencies are resolved */
          /* add their cost to our own */
          for (size_t j = 0; j < i->second.size(); ++j) {
            functionCosts[i->first] += functionCosts[i->second[j].to];
          }
          linkOrder.push_back(i->first);
          needResolve[i->first] = false;
          didResolve = true;
        }
      }
    }
  }

  /* update function call costs */
  //{
    //std::map<std::string, std::vector<FunctionCostTraverser::CallSite> >::const_iterator i = callMap.begin();
    //for ( ; i != callMap.end(); ++i) {
      //for (size_t j = 0; j < i->second.size(); ++j) {
        //operationCosts[i->second[j].from] = functionCosts[i->second[j].to];
      //}
    //}
  //}

  /* compute invocation counts */
  {
    std::vector<std::string>::const_reverse_iterator i = linkOrder.rbegin();
    for ( ; i != linkOrder.rend(); ++i) {
      size_t ic = (invocationCount[*i] ? invocationCount[*i] : 1);
      for (size_t j = 0; j < callMap[*i].size(); ++j) {
        invocationCount[callMap[*i][j].to] += loopCosts[callMap[*i][j].from] * ic;
      }
    }
  }

  /* update invocation counts */
  {
    std::map<TIntermNode*, size_t>::iterator i = operationCosts.begin();
    for ( ; i != operationCosts.end(); ++i) {
      size_t ic = invocationCount[functionMap[i->first]];
      i->second *= (ic ? ic : 1);
    }
  }

  return operationCosts;
}

CostCalculator::CostCalculator(const std::string& code) : shaderError_(false) {
  Shader shader = Shader::parseSource(code);
  shaderErrorString_ = shader.getInfoLog();
  if (!shaderErrorString_.empty()) {
    shaderError_ = true;
    shaderErrorString_ = shader.getInfoLog();
    return;
  }

  TIntermediate* tree = shader.getIntermediate();
  TIntermNode* root = tree->getTreeRoot();

  OperationCostTraverser oct;
  root->traverse(&oct);

  LoopCostTraverser lct;
  root->traverse(&lct);

  FunctionCostTraverser fct(lct.getCosts());
  root->traverse(&fct);

  std::map<TIntermNode*, size_t> operationCosts = calcOperationCosts(oct, lct, fct);
  CostSumTraverser operations(operationCosts);
  root->traverse(&operations);

  costs_ = operations.getCosts();
}
