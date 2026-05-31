#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
#define getcwd _getcwd
#define stat _stat
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

constexpr const char* kPwdPlaceholder = "${pwd}";

#if defined(_WIN32)
constexpr char kPathSeparator = '\\';
#else
constexpr char kPathSeparator = '/';
#endif

std::string replace_all(std::string out,
                        const std::string& placeholder,
                        const std::string& replacement) {
  std::string::size_type pos = 0;
  while ((pos = out.find(placeholder, pos)) != std::string::npos) {
    out.replace(pos, placeholder.size(), replacement);
    pos += replacement.size();
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  int first_arg_index = 1;
  if (argc > 1 && std::strcmp(argv[1], "--") == 0) {
    first_arg_index = 2;
  }

  if (first_arg_index >= argc) {
    std::fprintf(stderr, "bootstrap_process_wrapper: missing command\n");
    return 1;
  }

  char* pwd_raw = getcwd(nullptr, 0);
  if (pwd_raw == nullptr) {
    std::perror("bootstrap_process_wrapper: getcwd");
    return 1;
  }
  std::string pwd = pwd_raw;
  std::free(pwd_raw);

  std::vector<std::string> command_args;
  command_args.reserve(static_cast<size_t>(argc - first_arg_index));
  for (int i = first_arg_index; i < argc; ++i) {
    std::string arg = argv[i];
    arg = replace_all(arg, kPwdPlaceholder, pwd);
    command_args.push_back(arg);
  }

#if defined(_WIN32)
  // On Windows, build the command line manually with proper quoting
  // to handle paths with spaces (e.g. "C:\Program Files\...").
  for (char& c : command_args[0]) {
    if (c == '/') {
      c = '\\';
    }
  }

  // Build quoted command line string.
  std::string cmdline;
  for (size_t i = 0; i < command_args.size(); ++i) {
    if (i > 0) cmdline += ' ';
    const std::string& arg = command_args[i];
    bool needs_quote = arg.find(' ') != std::string::npos ||
                       arg.find('\t') != std::string::npos ||
                       arg.empty();
    if (needs_quote) cmdline += '"';
    cmdline += arg;
    if (needs_quote) cmdline += '"';
  }

  STARTUPINFOA si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};

  if (!CreateProcessA(
          nullptr,
          const_cast<char*>(cmdline.c_str()),
          nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
    std::fprintf(stderr, "bootstrap_process_wrapper: CreateProcess failed: %lu\n",
                 GetLastError());
    return 1;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exit_code = 1;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return static_cast<int>(exit_code);
#else
  std::vector<char*> exec_argv;
  exec_argv.reserve(command_args.size() + 1);
  for (const std::string& arg : command_args) {
    exec_argv.push_back(const_cast<char*>(arg.c_str()));
  }
  exec_argv.push_back(nullptr);

  execvp(exec_argv[0], exec_argv.data());
  std::perror("bootstrap_process_wrapper: execvp");
  return 1;
#endif
}
