#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define MAX_ARGS 64           // Maximum number of arguments in a command
#define DELIM " \t\r\n\a"     // Delimiters for splitting input

#define DEFAULT_HISTORY_SIZE 5  // Default size for command history

// Structure to store command history
typedef struct {
    char** commands;   // Array of command strings
    int size;          // Current number of commands in history
    int capacity;      // Maximum capacity of the history array
} History;

History history = {NULL, 0, DEFAULT_HISTORY_SIZE};  // Global history variable

// Structure for shell variables
typedef struct ShellVariable {
    char *name;                  // Name of the variable
    char *value;                 // Value of the variable
    struct ShellVariable *next;  // Pointer to the next variable in the list
} ShellVariable;

ShellVariable *shell_variables = NULL; // Head of the list of shell variables

// Function to display the shell prompt
void display_prompt() {
    printf("wsh> ");
}

// Function to read input from the user
char* read_input(void) {
    char *input = NULL;
    size_t bufsize = 0; 
    getline(&input, &bufsize, stdin);  // Read a line from stdin
    return input;
}

// Function to find a shell variable by its name
ShellVariable* find_shell_variable(char *name) {
    // Initialize a pointer to the head of the linked list of shell variables
    ShellVariable *current = shell_variables;
    
    // Traverse the linked list
    while (current != NULL) {
        // Compare the name of the current shell variable with the target name
        if (strcmp(current->name, name) == 0) {
            return current;  // Variable found, return the pointer to the ShellVariable structure
        }
        // Move to the next element in the linked list
        current = current->next;
    }
    
    // If the variable is not found, return NULL
    return NULL;
}


// Function to parse the input into an array of arguments
char** parse_input(char* input) {
    // Initialize the buffer size and position for storing tokens
    int bufsize = MAX_ARGS;
    int position = 0;
    // Allocate memory for the array of tokens
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    // Check for memory allocation error
    if (!tokens) {
        fprintf(stderr, "wsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    // Split the input into tokens based on the delimiter
    token = strtok(input, DELIM);
    while (token != NULL) {
        // Check if the token starts with a '$' indicating a variable
        if (token[0] == '$') {
            // Attempt to substitute the variable name with its value
            char *varName = token + 1; // Skip the '$'
            char *value = NULL;

            // Check for the variable in the environment variables
            value = getenv(varName);
            if (!value) {
                // If not found, check for the variable in the shell variables
                ShellVariable *var = find_shell_variable(varName);
                if (var) {
                    value = var->value;
                }
            }

            // If the variable is found, use its value; otherwise, use an empty string
            if (value) {
                tokens[position] = strdup(value);
            } else {
                tokens[position] = strdup("");
            }
        } else {
            // If the token does not start with '$', use it as is
            tokens[position] = strdup(token);
        }
        position++;

        // Resize the token array if necessary
        if (position >= bufsize) {
            bufsize += MAX_ARGS;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "wsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        // Get the next token
        token = strtok(NULL, DELIM);
    }
    // Null-terminate the array of tokens
    tokens[position] = NULL;
    return tokens;
}


// Function to execute a command
void execute_command(char **args) {
    pid_t pid;  // Process ID for the child process
    pid_t wpid; // Process ID for the waiting process
    int status; // Status of the child process

    pid = fork();  // Create a new process by duplicating the current process
    if (pid == 0) {
        // Child process
        // Replace the child process with a new program using execvp
        if (execvp(args[0], args) == -1) {
            // execvp failed, typically because the command was not found
            fprintf(stderr, "execvp: No such file or directory\n");
            exit(1); // Exit with an error status
        }
        // If execvp is successful, the child process does not return to this point
        exit(1); // Exit with an error status, in case execvp fails unexpectedly
    } else if (pid < 0) {
        // Forking failed, no child process was created
        perror("wsh"); // Print the error message
    } else {
        // Parent process
        do {
            // Wait for the child process to finish or be stopped
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        // Loop continues until the child process exits or is terminated by a signal
    }

    if (wpid > 0) { // Placeholder to avoid compiler warning, can be removed if not needed
    }
}


// Function to execute a command with piping
void execute_pipe_command(char **cmd1_args, char **cmd2_args) {
    int pipefd[2];  // Array to hold file descriptors for the pipe
    pid_t pid1, pid2;

    if (pipe(pipefd) == -1) {  // Create a pipe
        perror("wsh");
        return;
    }

    pid1 = fork();  // Fork first child process
    if (pid1 == 0) {
        // Child process for the first command
        dup2(pipefd[1], STDOUT_FILENO);  // Redirect standard output to the pipe
        close(pipefd[0]);  // Close unused read end of the pipe
        close(pipefd[1]);  // Close the write end of the pipe (after dup2)

        if (execvp(cmd1_args[0], cmd1_args) == -1) {  // Execute the first command
            fprintf(stderr, "execvp: No such file or directory\n");
            exit(1);
        }
        exit(1);
    } else if (pid1 < 0) {
        // Forking error for the first command
        perror("wsh");
        return;
    }

    pid2 = fork();  // Fork second child process
    if (pid2 == 0) {
        // Child process for the second command
        dup2(pipefd[0], STDIN_FILENO);  // Redirect standard input to the pipe
        close(pipefd[1]);  // Close unused write end of the pipe
        close(pipefd[0]);  // Close the read end of the pipe (after dup2)

        if (execvp(cmd2_args[0], cmd2_args) == -1) {  // Execute the second command
            fprintf(stderr, "execvp: No such file or directory\n");
            exit(1);
        }
        exit(1);
    } else if (pid2 < 0) {
        // Forking error for the second command
        perror("wsh");
        return;
    }

    close(pipefd[0]);  // Close the read end of the pipe in the parent process
    close(pipefd[1]);  // Close the write end of the pipe in the parent process

    waitpid(pid1, NULL, 0);  // Wait for the first child process to finish
    waitpid(pid2, NULL, 0);  // Wait for the second child process to finish
}

// Function to split a command string into two commands at the pipe character ('|')
int split_piped_commands(char* input, char** cmd1, char** cmd2) {
    // Search for the first occurrence of the pipe character in the input string
    char* pipe_position = strstr(input, "|");

    // If there is no pipe character in the input, set cmd1 to the entire input and return 1
    if (pipe_position == NULL) {
        *cmd1 = input;
        return 1;
    } else {
        // If a pipe character is found, split the input string into two commands
        *pipe_position = '\0';  // Replace the pipe character with a null terminator to end the first command
        *cmd1 = input;          // Set cmd1 to the beginning of the input (the first command)
        *cmd2 = pipe_position + 1;  // Set cmd2 to the character after the pipe (the second command)
        return 2;  // Return 2 to indicate that two commands were split
    }
}


// Function to execute multiple piped commands
void execute_multiple_pipe_commands(char **commands, int num_commands) {
    int i, in_fd = 0;  // Initialize the input file descriptor for the first command
    int fd[2];  // File descriptors for the pipe
    pid_t pid;  // Process ID

    // Allocate an array to store child process IDs
    pid_t *child_pids = malloc(num_commands * sizeof(pid_t));
    if (child_pids == NULL) {
        perror("malloc");
        exit(1);
    }

    for (i = 0; i < num_commands; i++) {
        // Create a pipe for all commands except the last one
        if (i < num_commands - 1) {
            if (pipe(fd) < 0) {
                perror("pipe");
                exit(1);
            }
        }

        pid = fork();  // Create a new process
        if (pid == 0) {  // Child process
            if (in_fd != STDIN_FILENO) {
                dup2(in_fd, STDIN_FILENO);  // Redirect input from the previous pipe
                close(in_fd);  // Close the old input file descriptor
            }
            if (i < num_commands - 1) {
                dup2(fd[1], STDOUT_FILENO);  // Redirect output to the next pipe
                close(fd[1]);  // Close the write end of the pipe
                close(fd[0]);  // Close the read end of the pipe
            }
            char **cmd_args = parse_input(commands[i]);  // Parse the command
            execvp(cmd_args[0], cmd_args);  // Execute the command
            // If execvp returns, it must have failed
            fprintf(stderr, "wsh: command not found: %s\n", cmd_args[0]);
            exit(1);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        } else {
            // Parent process
            // Store the pid of the child process
            child_pids[i] = pid;
            if (in_fd != STDIN_FILENO) {
                close(in_fd);  // Close the old input file descriptor
            }
            if (i < num_commands - 1) {
                close(fd[1]);  // Close the write end of the pipe
                in_fd = fd[0];  // Set up the read end of the pipe for the next command
            } else if (i == num_commands - 1) {
                close(fd[0]); // Close the last read end if it was opened
            }
        }
    }

    // Wait for all child processes to finish
    for (i = 0; i < num_commands; i++) {
        int status;
        waitpid(child_pids[i], &status, WUNTRACED);
    }

    // Free the allocated memory for child process IDs
    free(child_pids);
}

// Function prototypes for built-in shell commands
int wsh_cd(char **args);      // Change directory
int wsh_exit(char **args);    // Exit the shell
int wsh_export(char **args);  // Export a variable to the environment
int wsh_local(char **args);   // Set a local shell variable
int wsh_vars(char **args);    // List all shell variables
int handle_history_command(char **args); // Handle the history command

// Array of strings containing the names of the built-in commands
char *builtin_str[] = {
    "cd",
    "exit",
    "export",
    "local",
    "vars",
    "history"
};

// Array of function pointers corresponding to the built-in commands
int (*builtin_func[]) (char **) = {
    &wsh_cd,
    &wsh_exit,
    &wsh_export,
    &wsh_local,
    &wsh_vars,
    &handle_history_command
};


// Function to calculate the number of built-in commands
int wsh_num_builtins() {
    // Calculate the number of built-in commands by dividing the total size of the
    // builtin_str array by the size of a single element in the array.
    return sizeof(builtin_str) / sizeof(char *);
}

// Function to execute a built-in command
int execute_builtin(char **args) {
    // Check if the command is empty (i.e., no input was provided)
    if (args[0] == NULL) {
        // An empty command was entered, so nothing to execute.
        return 1;
    }

    // Iterate through the list of built-in commands to find a match
    for (int i = 0; i < wsh_num_builtins(); i++) {
        // Compare the input command with each built-in command
        if (strcmp(args[0], builtin_str[i]) == 0) {
            // If a match is found, execute the corresponding built-in function
            return (*builtin_func[i])(args);
        }
    }

    // The input command is not a built-in command
    return 0;
}


// Function to change the current directory
int wsh_cd(char **args) {
    // Check if the directory to change to is provided
    if (args[1] == NULL) {
        // If not, print an error message
        fprintf(stderr, "wsh: expected argument to \"cd\"\n");
    } else {
        // If yes, attempt to change the directory
        if (chdir(args[1]) != 0) {
            // If the change directory operation fails, print an error message
            perror("wsh");
        }
    }
    // Return 1 to indicate that the shell should continue running
    return 1;
}

// Function to exit the shell
int wsh_exit(char **args) {
    // Exit the program with status 0 (success)
    exit(0);
}

// Function to export a variable to the environment
int wsh_export(char **args) {
    // Check if the variable to export is provided
    if (args[1] == NULL) {
        // If not, print an error message
        fprintf(stderr, "wsh: expected argument to \"export\"\n");
        return 1;
    }

    // Split the argument into the variable name and value
    char *name = strtok(args[1], "=");
    char *value = strtok(NULL, "");

    // Check if the value is provided
    if (value == NULL || value[0] == '\0') {
        // If not, unset the environment variable
        unsetenv(name);
    } else if (value) {
        // If yes, set the environment variable
        if (setenv(name, value, 1) != 0) {
            // If the set environment variable operation fails, print an error message
            perror("wsh");
        }
    } else {
        // If the argument format is incorrect, print an error message
        fprintf(stderr, "wsh: export syntax error\n");
    }
    // Return 1 to indicate that the shell should continue running
    return 1;
}


// Function to set a shell variable
void set_shell_variable(char *name, char *value) {
    // Start at the beginning of the linked list of shell variables
    ShellVariable *current = shell_variables;
    ShellVariable *prev = NULL;

    // Iterate through the list to find if the variable already exists
    while (current != NULL) {
        // If the variable is found, update its value and return
        if (strcmp(current->name, name) == 0) {
            free(current->value);  // Free the old value
            current->value = strdup(value);  // Duplicate the new value
            return;
        }
        // Move to the next variable in the list
        prev = current;
        current = current->next;
    }

    // If the variable was not found, create a new variable
    ShellVariable *newVar = malloc(sizeof(ShellVariable));
    newVar->name = strdup(name);  // Duplicate the name
    newVar->value = strdup(value);  // Duplicate the value
    newVar->next = NULL;  // Set the next pointer to NULL

    // If the list is empty, set the new variable as the head of the list
    if (prev == NULL) {
        shell_variables = newVar;
    } else {
        // Otherwise, append the new variable to the end of the list
        prev->next = newVar;
    }
}

// Function to unset a shell variable
void unset_shell_variable(char *name) {
    // Use a pointer to a pointer to keep track of the link to the current variable
    ShellVariable **current = &shell_variables;
    while (*current != NULL) {
        ShellVariable *entry = *current;
        // If the variable is found, remove it from the list
        if (strcmp(entry->name, name) == 0) {
            *current = entry->next;  // Bypass the deleted variable
            // Free the memory allocated for the variable
            free(entry->name);
            free(entry->value);
            free(entry);
            return;
        }
        // Move to the next variable in the list
        current = &entry->next;
    }
}


// Function to handle the 'local' built-in command
int wsh_local(char **args) {
    // Check if the argument is provided
    if (args[1] == NULL) {
        fprintf(stderr, "wsh: expected argument to \"local\"\n");
        return 1;
    }

    // Duplicate the argument to safely use strtok
    char *arg = strdup(args[1]);
    // Split the argument into name and value parts
    char *name = strtok(arg, "=");
    char *value = strtok(NULL, "");

    // Check if the name is valid
    if (name == NULL) {
        fprintf(stderr, "wsh: invalid format for local variable assignment\n");
        free(arg); // Free the duplicated argument
        return 1;
    }

    // If value is NULL, unset the variable; otherwise, set the variable
    if (value == NULL) {
        unset_shell_variable(name);
    } else {
        set_shell_variable(name, value);
    }

    // Free the duplicated argument
    free(arg);
    return 1;
}

// Function to handle the 'vars' built-in command
int wsh_vars(char **args) {
    // Iterate through the linked list of shell variables
    ShellVariable *current = shell_variables;
    while (current != NULL) {
        // Print the name and value of each variable
        printf("%s=%s\n", current->name, current->value);
        // Move to the next variable in the list
        current = current->next;
    }
    return 1;
}


// Function to add a command to the history
void add_to_history(const char* command) {
    // Initialize history array if it's the first command
    if (history.commands == NULL) {
        history.commands = malloc(sizeof(char*) * history.capacity);
        for (int i = 0; i < history.capacity; i++) {
            history.commands[i] = NULL;
        }
    }

    // Shift existing commands down to make room for the new command at the top
    for (int i = history.size; i > 0; i--) {
        history.commands[i] = history.commands[i-1];
    }

    // Add the new command to the top of the history
    history.commands[0] = strdup(command);
    // Increment the size of the history, but don't exceed the capacity
    if (history.size < history.capacity) {
        history.size++;
    }
}

// Function to display the command history
void show_history() {
    for (int i = 0; i < history.size; i++) {
        printf("%d) %s\n", i + 1, history.commands[i]);
    }
}

// Function to set the size of the command history
void set_history_size(int new_size) {
    // Check for valid new size
    if (new_size < 0) {
        printf("Invalid history size.\n");
        return;
    }

    // Remove old commands if the new size is smaller than the current size
    while (history.size > new_size) {
        free(history.commands[history.size - 1]);
        history.commands[history.size - 1] = NULL;
        history.size--;
    }

    // Resize the history array
    char **new_history = realloc(history.commands, sizeof(char*) * new_size);
    if (!new_history && new_size > 0) {
        perror("Unable to resize history");
        return;
    }
    history.commands = new_history;
    history.capacity = new_size;

    // Initialize any new slots to NULL
    for (int i = history.size; i < new_size; i++) {
        history.commands[i] = NULL;
    }
}


// Function to execute a command from the history based on its index
void execute_history_command(int index) {
    // Check if the index is valid
    if (index < 1 || index > history.size) {
        printf("No such command in history.\n");
        return;
    }
    // Print and execute the command at the given index
    printf("Executing: %s\n", history.commands[index - 1]);
}

// Function to handle the 'history' built-in command
int handle_history_command(char** args) {
    // Check if the first argument is 'history'
    if (strcmp(args[0], "history") != 0) {
        return 0;
    }

    // If no additional arguments are provided, show the command history
    if (args[1] == NULL) {
        show_history();
        return 1;
    }

    // If the second argument is 'set' and a third argument is provided, set the history size
    if (strcmp(args[1], "set") == 0 && args[2] != NULL) {
        int new_size = atoi(args[2]);
        if (new_size > -1) {
            set_history_size(new_size);
            return 1;
        } else {
            printf("Invalid history size: %s\n", args[2]);
            return -1;
        }
    }

    // If the second argument is a number, execute the command at that index in the history
    int index = atoi(args[1]);
    if (index > 0) {
        execute_history_command(index);
        return 1;
    }

    // If the arguments are invalid, print an error message
    printf("Invalid history command or index: %s\n", args[1]);
    return -1;
}


// Function to execute commands from a file in batch mode
void run_batch_mode(const char *filename) {
    // Open the file for reading
    FILE *file = fopen(filename, "r");
    if (!file) {
        // If the file cannot be opened, print an error message and exit
        perror("wsh: fopen");
        exit(1);
    }

    // Variables for reading lines from the file
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char *line2;

    // Read lines from the file until the end of the file is reached
    while ((read = getline(&line, &len, file)) != -1) {
        // Replace the newline character at the end of the line with a null terminator
        if (line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        // Duplicate the line to avoid modifying the original buffer
        line2 = strdup(line);

        // Parse the input line into arguments
        char **args = parse_input(line2);

        // If the command is not a built-in command, execute it as an external command
        if (!execute_builtin(args)) {
            execute_command(args);
        }

        // Free the duplicated line after processing
        free(line2);
    }

    // Free the buffer used for reading lines and close the file
    free(line);
    fclose(file);
}


int main(int argc, char *argv[]) {
    // Variable declarations
    char *input;
    char **args;
    int status = 1;

    // Check if the program was run with a filename argument for batch mode
    if (argc > 1) {
        run_batch_mode(argv[1]); // Execute commands from the file
        return 0; // Exit after running batch mode
    }

    // Main loop for interactive mode
    do {
        display_prompt(); // Display the shell prompt
        input = read_input(); // Read a line of input from the user

        // Check for end-of-file (Ctrl+D) or empty input
        if (feof(stdin) || input == NULL || strlen(input) == 0) {
            if (input != NULL) {
                free(input); // Free the input buffer if it's not NULL
            }
            continue; // Skip to the next iteration of the loop
        }

        // Duplicate the input to avoid modifying the original buffer during parsing
        char *input2 = strdup(input);
        args = parse_input(input2); // Parse the input into arguments

        // Check for pipe commands
        if (strstr(input, "|")) {
            // Array to hold the commands to be piped
            char *commands[MAX_ARGS];
            int num_commands = 0;

            // Tokenize the input by the pipe symbol to separate commands
            char *command = strtok(input, "|");
            while (command != NULL && num_commands < MAX_ARGS) {
                commands[num_commands++] = command;
                command = strtok(NULL, "|");
            }

            // Execute the commands with piping
            execute_multiple_pipe_commands(commands, num_commands);
        } else {
            // If the command is not a built-in command, execute it as an external command
            if (!execute_builtin(args)) {
                execute_command(args);
                add_to_history(input); // Add the command to the history
            }
        }

        // Free the input buffers after processing
        free(input);
        free(input2);
    } while (status); // Continue looping until the status is set to 0 (exit)

    return 0; // Return success
}
