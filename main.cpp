#include "main.h"

struct Error {
	const std::string message;
	const std::string filePath;
	const std::string function;
	const std::string source;
	const std::string line;
};

static bool isCommandLine;
static bool isProgressBarActive = false;
static uint32_t filesSkipped = 0;

static struct {
	bool showHelp = false;
	bool silentAssertions = false;
	bool forceOverwrite = false;
	bool ignoreDebugInfo = false;
	bool minimizeDiffs = false;
	bool unrestrictedAscii = false;
	std::string inputPath;
	std::string outputPath;
	std::string extensionFilter;
} arguments;

struct Directory {
	const std::string path;
	std::vector<Directory> folders;
	std::vector<std::string> files;
};

static std::string string_to_lowercase(const std::string& string) {
	std::string lowercaseString = string;

	for (uint32_t i = lowercaseString.size(); i--;) {
		if (lowercaseString[i] < 'A' || lowercaseString[i] > 'Z') continue;
		lowercaseString[i] += 'a' - 'A';
	}

	return lowercaseString;
}

static std::string path_find_extension(const std::string& filename) {
	const std::size_t dot = filename.rfind('.');
	if (dot == std::string::npos) return "";
	return filename.substr(dot);
}

static std::string path_find_filename(const std::string& path) {
	const std::size_t sep = path.find_last_of("/\\");
	if (sep == std::string::npos) return path;
	return path.substr(sep + 1);
}

static std::string path_remove_extension(const std::string& filename) {
	const std::size_t dot = filename.rfind('.');
	if (dot == std::string::npos) return filename;
	return filename.substr(0, dot);
}

static void find_files_recursively(Directory& directory) {
	const std::string searchPath = arguments.inputPath + directory.path;

	try {
		for (const auto& entry : std::filesystem::directory_iterator(searchPath)) {
			if (entry.is_directory()) {
				std::string subPath = directory.path + entry.path().filename().string() + "/";
				directory.folders.emplace_back(Directory{ .path = subPath });
				find_files_recursively(directory.folders.back());
				if (!directory.folders.back().files.size() && !directory.folders.back().folders.size()) directory.folders.pop_back();
			} else if (entry.is_regular_file()) {
				const std::string filename = entry.path().filename().string();
				if (!arguments.extensionFilter.size() || arguments.extensionFilter == string_to_lowercase(path_find_extension(filename))) {
					directory.files.emplace_back(filename);
				}
			}
		}
	} catch (...) {
		return;
	}
}

static bool decompile_files_recursively(const Directory& directory) {
	std::filesystem::create_directories(arguments.outputPath + directory.path);
	std::string outputFile;

	for (uint32_t i = 0; i < directory.files.size(); i++) {
		outputFile = path_remove_extension(directory.files[i]) + ".lua";

		Bytecode bytecode(arguments.inputPath + directory.path + directory.files[i]);
		Ast ast(bytecode, arguments.ignoreDebugInfo, arguments.minimizeDiffs);
		Lua lua(bytecode, ast, arguments.outputPath + directory.path + outputFile, arguments.forceOverwrite, arguments.minimizeDiffs, arguments.unrestrictedAscii);

		try {
			print("--------------------\nInput file: " + bytecode.filePath + "\nReading bytecode...");
			bytecode();
			print("Building ast...");
			ast();
			print("Writing lua source...");
			lua();
			print("Output file: " + lua.filePath);
		} catch (const Error& error) {
			erase_progress_bar();

			print("\nError running " + error.function + "\nSource: " + error.source + ":" + error.line + "\n\nFile: " + error.filePath + "\n\n" + error.message);

			if (arguments.silentAssertions) {
				filesSkipped++;
				continue;
			}

			print("(s)kip, (r)etry, (a)bort?");
			char response = '\0';
			while (response != 's' && response != 'r' && response != 'a') {
				int c = getchar();
				if (c == EOF) { response = 'a'; break; }
				response = (char)c;
			}

			if (response == 'a') return false;
			if (response == 'r') { print("Retrying..."); i--; continue; }
			print("File skipped.");
			filesSkipped++;
		} catch (...) {
			erase_progress_bar();
			print("Unknown exception\n\nFile: " + bytecode.filePath);
			throw;
		}
	}

	for (uint32_t i = 0; i < directory.folders.size(); i++) {
		if (!decompile_files_recursively(directory.folders[i])) return false;
	}

	return true;
}

static char* parse_arguments(const int& argc, char** const& argv) {
	if (argc < 2) return nullptr;
	arguments.inputPath = argv[1];
#ifndef _DEBUG
	if (!isCommandLine) return nullptr;
#endif
	bool isInputPathSet = true;

	if (arguments.inputPath.size() && arguments.inputPath.front() == '-') {
		arguments.inputPath.clear();
		isInputPathSet = false;
	}

	std::string argument;

	for (uint32_t i = isInputPathSet ? 2 : 1; i < argc; i++) {
		argument = argv[i];

		if (argument.size() >= 2 && argument.front() == '-') {
			if (argument[1] == '-') {
				argument = argument.c_str() + 2;

				if (argument == "extension") {
					if (i <= argc - 2) {
						i++;
						arguments.extensionFilter = argv[i];
						continue;
					}
				} else if (argument == "force_overwrite") {
					arguments.forceOverwrite = true;
					continue;
				} else if (argument == "help") {
					arguments.showHelp = true;
					continue;
				} else if (argument == "ignore_debug_info") {
					arguments.ignoreDebugInfo = true;
					continue;
				} else if (argument == "minimize_diffs") {
					arguments.minimizeDiffs = true;
					continue;
				} else if (argument == "output") {
					if (i <= argc - 2) {
						i++;
						arguments.outputPath = argv[i];
						continue;
					}
				} else if (argument == "silent_assertions") {
					arguments.silentAssertions = true;
					continue;
				} else if (argument == "unrestricted_ascii") {
					arguments.unrestrictedAscii = true;
					continue;
				}
			} else if (argument.size() == 2) {
				switch (argument[1]) {
				case 'e':
					if (i > argc - 2) break;
					i++;
					arguments.extensionFilter = argv[i];
					continue;
				case 'f':
					arguments.forceOverwrite = true;
					continue;
				case '?':
				case 'h':
					arguments.showHelp = true;
					continue;
				case 'i':
					arguments.ignoreDebugInfo = true;
					continue;
				case 'm':
					arguments.minimizeDiffs = true;
					continue;
				case 'o':
					if (i > argc - 2) break;
					i++;
					arguments.outputPath = argv[i];
					continue;
				case 's':
					arguments.silentAssertions = true;
					continue;
				case 'u':
					arguments.unrestrictedAscii = true;
					continue;
				}
			}
		}

		return argv[i];
	}

	return nullptr;
}

static void wait_for_exit() {
	if (isCommandLine) return;
	print("Press enter to exit.");
	int c;
	while ((c = getchar()) != '\n' && c != EOF) {}
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
	{
		HWND window = GetConsoleWindow();
		DWORD consoleProcessId;
		GetWindowThreadProcessId(window, &consoleProcessId);
#ifdef _DEBUG
		isCommandLine = false;
#else
		isCommandLine = consoleProcessId != GetCurrentProcessId();
		if (!isCommandLine) SetWindowTextA(window, PROGRAM_NAME);
#endif
	}
#else
	isCommandLine = isatty(STDIN_FILENO);
#endif

	print(std::string(PROGRAM_NAME) + "\nCompiled on " + __DATE__);
	
	if (parse_arguments(argc, argv)) {
		print("Invalid argument: " + std::string(parse_arguments(argc, argv)) + "\nUse -? to show usage and options.");
		return EXIT_FAILURE;
	}
	
	if (arguments.showHelp) {
		print(
			"Usage: luajit-decompiler-v2 INPUT_PATH [options]\n"
			"\n"
			"Available options:\n"
			"  -h, -?, --help\t\tShow this message\n"
			"  -o, --output OUTPUT_PATH\tOverride default output directory\n"
			"  -e, --extension EXTENSION\tOnly decompile files with the specified extension\n"
			"  -s, --silent_assertions\tDisable assertion error pop-up window\n"
			"\t\t\t\t  and auto skip files that fail to decompile\n"
			"  -f, --force_overwrite\t\tAlways overwrite existing files\n"
			"  -i, --ignore_debug_info\tIgnore bytecode debug info\n"
			"  -m, --minimize_diffs\t\tOptimize output formatting to help minimize diffs\n"
			"  -u, --unrestricted_ascii\tDisable default UTF-8 encoding and string restrictions"
		);
		return EXIT_SUCCESS;
	}
	
	if (!arguments.inputPath.size()) {
		print("No input path specified!");
		return EXIT_FAILURE;
	}

	if (!arguments.outputPath.size()) {
#ifdef _WIN32
		char exePath[MAX_PATH];
		GetModuleFileNameA(NULL, exePath, MAX_PATH);
		*PathFindFileNameA(exePath) = '\0';
		arguments.outputPath = std::string(exePath) + "output\\";
#else
		char exePath[4096] = {};
		ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
		if (len > 0) {
			exePath[len] = '\0';
			char* slash = strrchr(exePath, '/');
			if (slash) *(slash + 1) = '\0';
			arguments.outputPath = std::string(exePath) + "output/";
		} else {
			arguments.outputPath = "output/";
		}
#endif
	} else {
		std::filesystem::file_status status = std::filesystem::status(arguments.outputPath);

		if (!std::filesystem::exists(status)) {
			print("Failed to open output path: " + arguments.outputPath);
			return EXIT_FAILURE;
		}

		if (!std::filesystem::is_directory(status)) {
			print("Output path is not a folder!");
			return EXIT_FAILURE;
		}

		char lastChar = arguments.outputPath.back();
		if (lastChar != '/' && lastChar != '\\') {
			arguments.outputPath += '/';
		}
	}

	if (arguments.extensionFilter.size()) {
		if (arguments.extensionFilter.front() != '.') arguments.extensionFilter.insert(arguments.extensionFilter.begin(), '.');
		arguments.extensionFilter = string_to_lowercase(arguments.extensionFilter);
	}

	std::filesystem::file_status inputStatus = std::filesystem::status(arguments.inputPath);

	if (!std::filesystem::exists(inputStatus)) {
		print("Failed to open input path: " + arguments.inputPath);
		wait_for_exit();
		return EXIT_FAILURE;
	}

	Directory root;

	if (std::filesystem::is_directory(inputStatus)) {
		char lastChar = arguments.inputPath.back();
		if (lastChar != '/' && lastChar != '\\') {
			arguments.inputPath += '/';
		}

		find_files_recursively(root);

		if (!root.files.size() && !root.folders.size()) {
			print("No files " + (arguments.extensionFilter.size() ? "with extension " + arguments.extensionFilter + " " : "") + "found in path: " + arguments.inputPath);
			wait_for_exit();
			return EXIT_FAILURE;
		}
	} else {
		root.files.emplace_back(path_find_filename(arguments.inputPath));
		const std::size_t sep = arguments.inputPath.find_last_of("/\\");
		arguments.inputPath = (sep == std::string::npos) ? "" : arguments.inputPath.substr(0, sep + 1);
	}

	try {
		if (!decompile_files_recursively(root)) {
			print("--------------------\nAborted!");
			wait_for_exit();
			return EXIT_FAILURE;
		}
	} catch (...) {
		throw;
	}

#ifndef _DEBUG
	print("--------------------\n" + (filesSkipped ? "Failed to decompile " + std::to_string(filesSkipped) + " file" + (filesSkipped > 1 ? "s" : "") + ".\n" : "") + "Done!");
	wait_for_exit();
#endif
	return EXIT_SUCCESS;
}

void print(const std::string& message) {
	fputs((message + '\n').c_str(), stdout);
	fflush(stdout);
}

/*
std::string input() {
	static char BUFFER[1024];
	return fgets(BUFFER, sizeof(BUFFER), stdin) ? std::string(BUFFER) : "";
}
*/

void print_progress_bar(const double& progress, const double& total) {
	static char PROGRESS_BAR[] = "\r[====================]";

	const uint8_t threshold = std::round(20 / total * progress);

	for (uint8_t i = 20; i--;) {
		PROGRESS_BAR[i + 2] = i < threshold ? '=' : ' ';
	}

	fwrite(PROGRESS_BAR, 1, sizeof(PROGRESS_BAR) - 1, stdout);
	fflush(stdout);
	isProgressBarActive = true;
}

void erase_progress_bar() {
	static constexpr char PROGRESS_BAR_ERASER[] = "\r                      \r";

	if (!isProgressBarActive) return;
	fwrite(PROGRESS_BAR_ERASER, 1, sizeof(PROGRESS_BAR_ERASER) - 1, stdout);
	fflush(stdout);
	isProgressBarActive = false;
}

void assert(const bool& assertion, const std::string& message, const std::string& filePath, const std::string& function, const std::string& source, const uint32_t& line) {
	if (!assertion) throw Error{
		.message = message,
		.filePath = filePath,
		.function = function,
		.source = source,
		.line = std::to_string(line)
	};
}

std::string byte_to_string(const uint8_t& byte) {
	char string[] = "0x00";
	uint8_t digit;
	
	for (uint8_t i = 2; i--;) {
		digit = (byte >> i * 4) & 0xF;
		string[3 - i] = digit >= 0xA ? 'A' + digit - 0xA : '0' + digit;
	}

	return string;
}
