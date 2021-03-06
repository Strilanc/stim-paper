// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "arg_parse.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

const char *require_find_argument(const char *name, int argc, const char **argv) {
    const char *result = find_argument(name, argc, argv);
    if (result == 0) {
        fprintf(stderr, "\033[31mMissing command line argument: '%s'\033[0m\n", name);
        exit(EXIT_FAILURE);
    }
    return result;
}

const char *find_argument(const char *name, int argc, const char **argv) {
    // Respect that the "--" argument terminates flags.
    size_t flag_count = 1;
    while (flag_count < (size_t)argc && strcmp(argv[flag_count], "--") != 0) {
        flag_count++;
    }

    // Search for the desired flag.
    size_t n = strlen(name);
    for (size_t i = 1; i < flag_count; i++) {
        // Check if argument starts with expected flag.
        const char *loc = strstr(argv[i], name);
        if (loc != argv[i] || (loc[n] != '\0' && loc[n] != '=')) {
            continue;
        }

        // If the flag is alone and followed by the end or another flag, no
        // argument was provided. Return the empty string to indicate this.
        if (loc[n] == '\0' && ((int)i == argc - 1 || argv[i + 1][0] == '-')) {
            return argv[i] + n;
        }

        // If the flag value is specified inline with '=', return a pointer to
        // the start of the value within the flag string.
        if (loc[n] == '=') {
            // Argument provided inline.
            return loc + n + 1;
        }

        // The argument value is specified by the next command line argument.
        return argv[i + 1];
    }

    // Not found.
    return 0;
}

void check_for_unknown_arguments(
    const std::vector<const char *> &known_arguments, const char *for_mode, int argc, const char **argv) {
    for (int i = 1; i < argc; i++) {
        // Respect that the "--" argument terminates flags.
        if (!strcmp(argv[i], "--")) {
            break;
        }

        // Check if there's a matching command line argument.
        int matched = 0;
        for (size_t j = 0; j < known_arguments.size(); j++) {
            const char *loc = strstr(argv[i], known_arguments[j]);
            size_t n = strlen(known_arguments[j]);
            if (loc == argv[i] && (loc[n] == '\0' || loc[n] == '=')) {
                // Skip words that are values for a previous flag.
                if (loc[n] == '\0' && i < argc - 1 && argv[i + 1][0] != '-') {
                    i++;
                }
                matched = 1;
                break;
            }
        }

        // Print error and exit if flag is not recognized.
        if (!matched) {
            if (for_mode == nullptr) {
                fprintf(stderr, "\033[31mUnrecognized command line argument %s.\n", argv[i]);
                fprintf(stderr, "Recognized command line arguments:\n");
            } else {
                fprintf(stderr, "\033[31mUnrecognized command line argument %s for mode %s.\n", argv[i], for_mode);
                fprintf(stderr, "Recognized command line arguments for mode %s:\n", for_mode);
            }
            for (size_t j = 0; j < known_arguments.size(); j++) {
                fprintf(stderr, "    %s\n", known_arguments[j]);
            }
            fprintf(stderr, "\033[0m");
            exit(EXIT_FAILURE);
        }
    }
}

bool find_bool_argument(const char *name, int argc, const char **argv) {
    const char *text = find_argument(name, argc, argv);
    if (text == nullptr) {
        return false;
    }
    if (text[0] == '\0') {
        return true;
    }
    fprintf(stderr, "\033[31mGot non-empty value '%s' for boolean flag '%s'.\033[0m\n", text, name);
    exit(EXIT_FAILURE);
}

int find_int_argument(const char *name, int default_value, int min_value, int max_value, int argc, const char **argv) {
    const char *text = find_argument(name, argc, argv);
    if (text == nullptr || text[0] == '\0') {
        if (default_value < min_value || default_value > max_value) {
            fprintf(
                stderr,
                "\033[31m"
                "Must specify a value for int flag '%s'.\n"
                "\033[0m",
                name);
            exit(EXIT_FAILURE);
        }
        return default_value;
    }

    // Attempt to parse.
    char *processed;
    long i = strtol(text, &processed, 10);
    if (*processed != '\0') {
        fprintf(stderr, "\033[31mGot non-integer value '%s' for integer flag '%s'.\033[0m\n", text, name);
        exit(EXIT_FAILURE);
    }

    // In range?
    if (i < min_value || i > max_value) {
        fprintf(
            stderr, "\033[31mInteger value '%s' for flag '%s' doesn't satisfy %d <= %ld <= %d.\033[0m\n", text, name,
            min_value, i, max_value);
        exit(EXIT_FAILURE);
    }

    return (int)i;
}

float find_float_argument(
    const char *name, float default_value, float min_value, float max_value, int argc, const char **argv) {
    const char *text = find_argument(name, argc, argv);
    if (text == nullptr) {
        if (default_value < min_value || default_value > max_value) {
            fprintf(
                stderr,
                "\033[31m"
                "Must specify a value for float flag '%s'.\n"
                "\033[0m",
                name);
            exit(EXIT_FAILURE);
        }
        return default_value;
    }

    // Attempt to parse.
    char *processed;
    float f = strtof(text, &processed);
    if (*processed != '\0') {
        fprintf(stderr, "\033[31mGot non-float value '%s' for float flag '%s'.\033[0m\n", text, name);
        exit(EXIT_FAILURE);
    }

    // In range?
    if (f < min_value || f > max_value || f != f) {
        fprintf(
            stderr, "\033[31mFloat value '%s' for flag '%s' doesn't satisfy %f <= %f <= %f.\033[0m\n", text, name,
            min_value, f, max_value);
        exit(EXIT_FAILURE);
    }

    return f;
}

int find_enum_argument(
    const char *name, int default_index, const std::vector<const char *> &known_values, int argc, const char **argv) {
    const char *text = find_argument(name, argc, argv);
    if (text == nullptr) {
        if (default_index >= 0) {
            return default_index;
        }
        fprintf(stderr, "\033[31mMust specify a value for enum flag '%s'.\n", name);
    } else {
        for (size_t i = 0; i < known_values.size(); i++) {
            if (!strcmp(text, known_values[i])) {
                return i;
            }
        }
        fprintf(stderr, "\033[31mUnrecognized value '%s' for enum flag '%s'.\n", text, name);
    }

    fprintf(stderr, "Recognized values are:\n");
    for (size_t i = 0; i < known_values.size(); i++) {
        fprintf(stderr, "    '%s'%s\n", known_values[i], i == (size_t)default_index ? " (default)" : "");
    }
    fprintf(stderr, "\033[0m");
    exit(EXIT_FAILURE);
}
