#include <ParseHelper.h>
#include "CostCalculator.h"
#include <algorithm>

std::string colour(const int cost, const int maxCost)
{
  float n = cost / float(maxCost);
  //n = pow(n, 2.0);
  int ac[] = {255, 255, 255};
  int bc[] = {255, 0, 0};
  int cc[] = {
    int(ac[0]*(1-n) + bc[0]*n),
    int(ac[1]*(1-n) + bc[1]*n),
    int(ac[2]*(1-n) + bc[2]*n)
  };
  if (!cost) {
    cc[0]=cc[1]=cc[2]=255;
  }
  char buffer[64];
  sprintf(buffer, "%02X%02X%02X", cc[0], cc[1], cc[2]);
  return std::string() + "background-color:#" + buffer;
}

std::string htmlEscape(const std::string& input)
{
  struct Escape {
    const char* match;
    const char* replace;
  };

  const Escape escape[] = {
    {" ", "&nbsp;"},
    {"<", "&#60;"},
    {">", "&#62;"}
  };

  std::string line = input;

  for (size_t i = 0; i < sizeof(escape)/sizeof(*escape); ++i) {
    while (line.find(escape[i].match) != std::string::npos) {
      size_t idx = line.find(escape[i].match);
      line = line.substr(0, idx) + escape[i].replace + line.substr(idx+1);
    }
  }

  return line;
}

void formatOutput(const std::string& code, const std::map<int, int>& costs) {

  int maxCost = 0;
  std::map<int, int>::const_iterator i = costs.begin();
  for (; i != costs.end(); ++i) {
    if (i->second > maxCost) {
      maxCost = i->second;
    }
  }

  FILE* file = fopen("output.html", "w");
  if (!file) {
    return;
  }
  fprintf(file, "<html><table>\n");
  std::string text = code;
  int lcount = 1;
  while (!text.empty()) {
    std::string line;
    size_t lend = text.find("\n");
    if (lend != std::string::npos) {
      line = text.substr(0, lend);
      text = text.substr(lend+1);
    } else {
      line = text;
      text = "";
    }

    line = htmlEscape(line);

    int lineCost = 0;
    char costString[64];
    sprintf(costString, "0");
    std::map<int, int>::const_iterator i = costs.find(lcount);
    if (i != costs.end()) {
      lineCost = i->second;
      sprintf(costString, "%d", lineCost);
    }

    std::string style = "font-family:'Courier New'; " + colour(lineCost, maxCost);
    std::string tago = "<p style=\"" + style + "\">\n";
    std::string tagc = "</p>\n";

    std::string pstyle = "font-family:'Courier New'; ";
    std::string ptago = "<p style=\"" + pstyle + "\">\n";
    std::string ptagc = "</p>\n";

    std::string content = tago + line + tagc;
    std::string costContent = ptago + costString + ptagc;
    fprintf(file, "<tr><td>%s%d%s</td><td>%s</td><td>%s</td><tr>\n", ptago.c_str(), lcount, ptagc.c_str(), content.c_str(), costContent.c_str());

    lcount++;
  }
  fprintf(file, "</p>\n");
  fprintf(file, "</table></html>\n");
  fclose(file);
}

int calc(const std::string& code) {
  CostCalculator calculator(code);
  if (calculator.shaderError()) {
    const std::string& error = calculator.shaderErrorString();
    printf("%s", error.c_str());
    return -1;
  }
  formatOutput(code, calculator.getCosts());
  return 0;
}

int loadAndCalc(const std::string& path) {
  FILE* file = fopen(path.c_str(), "r");
  if (file) {
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = (char*)malloc(size+1);
    if (buffer) {
      size_t read = fread(buffer, 1, size, file);
      buffer[size] = '\0';
      if (read == size) {
        int ret = calc(buffer);
        return ret;
      } else {
        printf("Error: Failed to read input file.\n");
        return -1;
      }
    } else {
      printf("Error: Failed to allocate memory.\n");
      return -1;
    }
    free(buffer);
    fclose(file);
  } else {
    printf("Error: Failed to open input file for reading.\n");
    return -1;
  }
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf("Usage: %s <file>\n", argv[0]);
    return 0;
  }
  ShInitialize();
  int ret = loadAndCalc(argv[1]);
  ShFinalize();
  return ret;
}
