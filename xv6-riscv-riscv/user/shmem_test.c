#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define TEST_SIZE 4096
#define TEST_STRING "Hello daddy"

void print_size(char *label, int pid) {
    char *current_break = sbrk(0);
    printf("%s (PID %d): size = %p\n", label, pid, current_break);
}

int main(int argc, char *argv[]) {
    int test_no_unmap = 0;
    int pipefd[2];
    
    if (argc > 1 && strcmp(argv[1], "no_unmap") == 0) {
        test_no_unmap = 1;
        printf("Running test with no unmapping in child\n");
    }
    
    printf("=== Shared Memory Test ===\n");
    
    // Create pipe for communication
    if (pipe(pipefd) < 0) {
        printf("pipe failed\n");
        exit(1);
    }
    
    // Allocate memory to share
    char *shared_buffer = sbrk(TEST_SIZE);
    if (shared_buffer == (char*)-1) {
        printf("sbrk failed\n");
        exit(1);
    }
    
    // Initialize buffer
    for (int i = 0; i < TEST_SIZE; i++) {
        shared_buffer[i] = 0;
    }
    
    printf("Parent allocated buffer at %p\n", shared_buffer);
    print_size("Parent before fork", getpid());
    
    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    }
    
    if (pid == 0) {
        // Child process
        close(pipefd[1]); // Close write end
        
        printf("\nChild process started (PID %d)\n", getpid());
        print_size("Child before mapping", getpid());
        
        // Read shared memory address from parent
        uint64 shared_addr;
        if (read(pipefd[0], &shared_addr, sizeof(shared_addr)) != sizeof(shared_addr)) {
            printf("Child: failed to read shared address\n");
            exit(1);
        }
        close(pipefd[0]);
        
        printf("Child: received shared memory at %p\n", (void*)shared_addr);
        print_size("Child after mapping", getpid());
        
        // Write to shared memory
        char *child_shared = (char*)shared_addr;
        strcpy(child_shared, TEST_STRING);
        printf("Child: wrote '%s' to shared memory\n", TEST_STRING);
        
        if (!test_no_unmap) {
            // Unmap shared memory
            if (unmap_shared_pages(shared_addr, TEST_SIZE) != 0) {
                printf("Child: unmap_shared_pages failed\n");
                exit(1);
            }
            printf("Child: unmapped shared memory\n");
            print_size("Child after unmapping", getpid());
            
            // Test malloc after unmapping
            char *test_malloc = malloc(1024);
            if (test_malloc == 0) {
                printf("Child: malloc failed after unmapping\n");
                exit(1);
            }
            printf("Child: malloc succeeded after unmapping\n");
            print_size("Child after malloc", getpid());
            free(test_malloc);
        } else {
            printf("Child: skipping unmap (testing cleanup on exit)\n");
        }
        
        printf("Child exiting\n");
        exit(0);
    } else {
        // Parent process
        close(pipefd[0]); // Close read end
        
        printf("\nParent mapping memory to child (PID %d)\n", pid);
        
        // Map our buffer to child
        uint64 child_addr = map_shared_pages(pid, (uint64)shared_buffer, TEST_SIZE);
        if (child_addr == (uint64)-1) {
            printf("Parent: map_shared_pages failed\n");
            exit(1);
        }
        
        printf("Parent: mapped buffer to child at address %p\n", (void*)child_addr);
        
        // Send address to child
        if (write(pipefd[1], &child_addr, sizeof(child_addr)) != sizeof(child_addr)) {
            printf("Parent: failed to send shared address\n");
            exit(1);
        }
        close(pipefd[1]);
        
        // Give child time to write
        sleep(2);
        
        printf("\nParent reading from buffer: '%s'\n", shared_buffer);
        
        // Wait for child
        int xstatus;
        wait(&xstatus);
        
        if (xstatus != 0) {
            printf("Child exited with error status %d\n", xstatus);
        } else {
            printf("Child completed successfully\n");
        }
        
        printf("Parent final read from buffer: '%s'\n", shared_buffer);
        print_size("Parent after child exit", getpid());
    }
    
    printf("\n=== Test completed ===\n");
    exit(0);
}