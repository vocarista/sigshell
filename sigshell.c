#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <termios.h> // For tcsetpgrp

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

// Global variable for terminal's controlling process group ID
pid_t shell_pgid;
struct termios shell_tmodes;

// Signal handler for SIGINT (Ctrl+C) in parent shell
void sigint_handler(int sig) {
    printf("\n[Shell] Use 'exit' command to quit the shell.\n");
    printf("sigshell> ");
    fflush(stdout);
}

// Parse command line into arguments
int parse_command(char *cmd, char **args) {
    int i = 0;
    // Use strsep for safer tokenization if required, but strtok is fine for this example.
    char *token = strtok(cmd, " \t\n");
    
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return i;
}

// Check if command should have SIGINT protection
int should_protect_sigint(char *cmd) {
    const char *protected[] = {"sleep", "critical", NULL};
    
    for (int i = 0; protected[i] != NULL; i++) {
        if (strcmp(cmd, protected[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Execute a command
void execute_command(char **args, int protect_sigint) {
    pid_t pid;
    
    // Check if the shell is interactive (has a controlling terminal)
    if (isatty(STDIN_FILENO)) {
        // Create a new process group for the child before forking
        // This is the ideal place to do setpgid(-1, 0)
        // For simplicity here, we'll set it in the child using setpgid(0, 0)
    }

    pid = fork();
    
    if (pid < 0) {
        perror("fork failed");
        return;
    }
    
    if (pid == 0) {
        // Child process
        
        // 1. Give the child process its own process group
        setpgid(0, 0); 
        
        // 2. Setup signal handling for the child
        struct sigaction sa;
        
        // Restore default SIGTSTP behavior for child
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGTSTP, &sa, NULL);

        // Handle SIGINT protection
        if (protect_sigint) {
            // Make child ignore SIGINT
            sa.sa_handler = SIG_IGN;
            sigaction(SIGINT, &sa, NULL);
            printf("[Child] This process will ignore Ctrl+C (PID: %d)\n", getpid());
        } else {
            // Restore default SIGINT behavior for child
            sa.sa_handler = SIG_DFL;
            sigaction(SIGINT, &sa, NULL);
        }
        
        // Execute the command
        if (execvp(args[0], args) < 0) {
            perror("Command execution failed");
            exit(1);
        }
    } else {
        // Parent process (Shell)
        int status;
        pid_t child_pgid = pid; // Use child PID as its PGID for tcsetpgrp

        if (protect_sigint) {
            printf("[Shell] Process %d is protected from SIGINT (Ctrl+C won't work)\n", pid);
        }

        // 1. Give the child's process group control of the terminal
        if (isatty(STDIN_FILENO)) {
            tcsetpgrp(STDIN_FILENO, child_pgid);
        }
        
        // 2. Wait for child to complete, allowing it to be stopped
        pid_t result = waitpid(pid, &status, WUNTRACED);
        
        if (result > 0) {
            if (WIFSTOPPED(status)) {
                // Process was stopped by SIGTSTP
                printf("\n[Shell] Process %d suspended.\n", pid);
                printf("[Shell] Use 'kill -CONT %d' to resume it (or a job control command in a real shell).\n", pid);
            } else if (WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                if (exit_status != 0) {
                    printf("[Shell] Process exited with status %d\n", exit_status);
                }
            } else if (WIFSIGNALED(status)) {
                printf("[Shell] Process terminated by signal %d\n", WTERMSIG(status));
            }
        } else if (result == -1) {
            perror("waitpid failed");
        }
        
        // 3. Reclaim terminal control
        if (isatty(STDIN_FILENO)) {
            tcsetpgrp(STDIN_FILENO, shell_pgid);
        }
    }
}

// Built-in commands
int handle_builtin(char **args) {
    if (args[0] == NULL) {
        return 1;
    }
    
    if (strcmp(args[0], "exit") == 0) {
        printf("Goodbye!\n");
        return 2; // Special return value to exit shell
    }
    
    if (strcmp(args[0], "help") == 0) {
        printf("\n=== Custom Signal Handling Shell ===\n");
        printf("Features:\n");
        printf("  - Ctrl+C in shell shows message instead of exiting\n");
        printf("  - 'sleep' commands ignore Ctrl+C (SIGINT protected)\n");
        printf("  - Ctrl+Z suspends process directly (proper job control set up)\n");
        printf("\nBuilt-in commands:\n");
        printf("  help     - Show this help message\n");
        printf("  exit     - Exit the shell\n");
        printf("  cd <dir> - Change directory\n");
        printf("\nTry these:\n");
        printf("  sleep 10     - Try pressing Ctrl+C (won't work!)\n");
        printf("  ls -la       - Try pressing Ctrl+C (will work)\n");
        printf("  cat          - Try pressing Ctrl+Z (will suspend)\n");
        printf("\n");
        return 1;
    }
    
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
        } else {
            if (chdir(args[1]) != 0) {
                perror("cd failed");
            }
        }
        return 1;
    }
    
    return 0; // Not a built-in command
}

// Initialization for job control
void init_shell() {
    // Check if the shell is running interactively
    if (isatty(STDIN_FILENO)) {
        // Loop until we are in the foreground
        while (tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp())) {
            kill(-shell_pgid, SIGTTIN);
        }

        // Ignore SIGINT and SIGTSTP while in the shell's main loop
        signal(SIGINT, sigint_handler);
        signal(SIGQUIT, SIG_IGN); // Ignore Quit signal
        signal(SIGTSTP, SIG_IGN); // Ignore Stop signal (Ctrl+Z)
        signal(SIGTTIN, SIG_IGN); // Ignore Read from terminal for background
        signal(SIGTTOU, SIG_IGN); // Ignore Write to terminal for background

        // Put ourselves in our own process group
        setpgid(shell_pgid, shell_pgid);
        
        // Grab control of the terminal
        tcsetpgrp(STDIN_FILENO, shell_pgid);

        // Save default terminal attributes for later
        tcgetattr(STDIN_FILENO, &shell_tmodes);
    }
}

int main() {
    char cmd[MAX_CMD_LEN];
    char *args[MAX_ARGS];
    
    // Setup for Job Control
    init_shell();
    
    // The previous signal setup with sigaction is technically redundant now
    // due to the simple 'signal()' calls in init_shell(), but is fine.
    // We rely on 'init_shell' for the crucial SIG_IGN settings.

    printf("\n=== Custom Signal Handling Shell ===\n");
    printf("Type 'help' for usage information.\n");
    printf("Type 'exit' to quit.\n\n");
    
    while (1) {
        printf("sigshell> ");
        fflush(stdout);
        
        // Read command
        if (fgets(cmd, sizeof(cmd), stdin) == NULL) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            // An interrupted read (e.g., SIGINT) is now handled by SA_RESTART/SIG_DFL, 
            // but the loop ensures the prompt is printed again.
            continue;
        }
        
        // Remove trailing newline
        cmd[strcspn(cmd, "\n")] = 0;
        
        // Skip empty commands
        if (strlen(cmd) == 0) {
            continue;
        }
        
        // Parse command
        int argc = parse_command(cmd, args);
        if (argc == 0) {
            continue;
        }
        
        // Handle built-in commands
        int builtin_result = handle_builtin(args);
        if (builtin_result == 2) {
            break; // Exit command
        } else if (builtin_result == 1) {
            continue; // Other built-in handled
        }
        
        // Check if command should be protected from SIGINT
        int protect = should_protect_sigint(args[0]);
        
        // Execute external command
        execute_command(args, protect);
    }
    
    return 0;
}