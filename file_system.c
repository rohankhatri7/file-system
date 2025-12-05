#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// System constraints
#define MAX_NAME_LEN 64  // max length of directory name      
#define MAX_NODES 128    // total nodes available (files + all directories)
#define INPUT_BUFFER 512 // the input size of the shell command

// Ram Disk parameters
#define BLOCK_SIZE 512        // Size of one data block in bytes
#define TOTAL_BLOCKS 4096     // Total disk size roughly 2MB
#define DEFAULT_FILE_ALLOC 4  // Simple policy: allocate 2KB per new file

/* Simulated Hardware */
// The raw storage area
static unsigned char vDisk[BLOCK_SIZE * TOTAL_BLOCKS];
// Allocation map: 0 = free, 1 = used. 
static unsigned char freeBlockMap[TOTAL_BLOCKS];

/* Data Structure */
typedef struct {
    bool inUse;                 
    bool isDirectory;           
    char name[MAX_NAME_LEN];   
    int parentIndex;         
    bool isOpen;
    int startBlockPtr;         
    int blockCount;           
    int sizeBytes;           
} FileSystemNode;

// The master table of all files and directories (flat location, logical tree)
static FileSystemNode NodeTable[MAX_NODES];

// Global System State
static int currentDirectoryIndex = 0; // starts at root (/), which will be index 0

// cleans newline characters from fgets input
static void trim_newline(char *s) {
    if (!s) {
			return;
		}

    s[strcspn(s, "\n")] = '\0';
}

// returns starting block index, or -1 if disk is full/fragmented.
static int allocate_disk_blocks(int blocksNeeded) {
    if (blocksNeeded <= 0) {
			return -1;
		}

    int consecutiveFound = 0;
    int potentialStart = -1;

    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (freeBlockMap[i] == 0) {
            if (consecutiveFound == 0) {
							potentialStart = i;
						}
            consecutiveFound++;
            if (consecutiveFound == blocksNeeded) {
                // Found space, mark as used
                for (int j = 0; j < blocksNeeded; j++) {
                    freeBlockMap[potentialStart + j] = 1;
                    // Zero out the simulated disk memory for safety
                    memset(&vDisk[(potentialStart + j) * BLOCK_SIZE], 0, BLOCK_SIZE);
                }
                return potentialStart;
            }
        } else {
            consecutiveFound = 0;
        }
    }

    return -1;
}

// finds a free slot in the metadata table.
static int find_free_node_slot(void) {
    for (int i = 1; i < MAX_NODES; i++) { // Start at 1, 0 is reserved for Root
        if (!NodeTable[i].inUse) {
					return i;
				}
    }

    return -1;
}

// searches for a specific name *only within the current directory*.
static int find_node_in_current_dir(const char *name) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (NodeTable[i].inUse && NodeTable[i].parentIndex == currentDirectoryIndex && strcmp(NodeTable[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// recursive helper to print the full path for the prompt
static void print_path_recursive(int nodeIdx) {
    if (nodeIdx == 0) {
        printf("/");
        return;
    }
    print_path_recursive(NodeTable[nodeIdx].parentIndex);
    if (NodeTable[NodeTable[nodeIdx].parentIndex].parentIndex != 0) {
         printf("/"); // Don't double slash after root
    }
    printf("%s", NodeTable[nodeIdx].name);
}

/* System Initialization */
static void init_filesystem(void) {
    // Clear all memory
    memset(NodeTable, 0, sizeof(NodeTable));
    memset(freeBlockMap, 0, sizeof(freeBlockMap));
    // vDisk doesn't necessarily need clearing until blocks are alloc'd

    // Create the Root Directory at index 0
    NodeTable[0].inUse = true;
    NodeTable[0].isDirectory = true;
    strcpy(NodeTable[0].name, "root");
    NodeTable[0].parentIndex = 0; // Root is its own parent
    currentDirectoryIndex = 0;

    printf("System initialized and root directory created.\n");
}

/* Core Commands */
static void cmd_help(void) {
    printf("--- Available Commands ---\n");
    printf("  ls           : List contents of current directory\n");
    printf("  mkdir <name> : Create a new directory\n");
    printf("  cd <name>    : Change directory (use '..' for parent folder)\n");
    printf("  touch <name> : Create a new file\n");
    printf("  open <name>  : Open a file for I/O\n");
    printf("  write <name> : Write text content to an open file\n");
    printf("  read <name>  : Display content of an open file\n");
    printf("  close <name> : Close a file\n");
    printf("  exit         : Shutdown the simulation\n");
    printf("--------------------------\n");
}

static void cmd_ls(void) {
    int count = 0;
    printf("Contents of directory:\n");
    for (int i = 0; i < MAX_NODES; i++) {
        // must be inUse AND belong to the current directory pointer
        if (!NodeTable[i].inUse || NodeTable[i].parentIndex != currentDirectoryIndex) {
            continue;
				}

        count++;
        if (NodeTable[i].isDirectory) {
            printf("  [DIR]  %s/\n", NodeTable[i].name);
        } else {
            printf("  [FILE] %-16s | Size: %5d bytes | %s\n",
                   NodeTable[i].name,
                   NodeTable[i].sizeBytes,
                   (NodeTable[i].isOpen ? "(OPEN)" : ""));
        }
    }

    if (count == 0) {
			printf("  (empty)\n");
		}
}

// make directory
static void cmd_mkdir(const char *name) {
    if (!name || !*name) {
			printf("Error: Missing directory name.\n");
			return;
		}

    if (find_node_in_current_dir(name) != -1) {
			printf("Error: Name already exists here.\n");
			return;
		}

    int slot = find_free_node_slot();
    if (slot < 0) { 
			printf("Error: Max node limit reached (directory table full).\n");
			return;
		}

    // Set up directory data
    NodeTable[slot].inUse = true;
    NodeTable[slot].isDirectory = true;
    strncpy(NodeTable[slot].name, name, MAX_NAME_LEN - 1);
    NodeTable[slot].name[MAX_NAME_LEN - 1] = '\0';
    NodeTable[slot].parentIndex = currentDirectoryIndex;

    printf("Directory '%s/' created.\n", NodeTable[slot].name);
}

// change directory
static void cmd_cd(const char *name) {
    if (!name || !*name) {
			printf("Error: Missing destination.\n");
			return; 
		}

    // Handle parent directory traversal
    if (strcmp(name, "..") == 0) {
        currentDirectoryIndex = NodeTable[currentDirectoryIndex].parentIndex;
        return;
    }
    
    // Handle going to root
    if (strcmp(name, "/") == 0) {
        currentDirectoryIndex = 0;
        return;
    }

    int idx = find_node_in_current_dir(name);
    if (idx < 0) {
        printf("Error: Directory '%s' not found.\n", name);
        return;
    }

    if (!NodeTable[idx].isDirectory) {
        printf("Error: '%s' is a file, not a directory.\n", name);
        return;
    }

    // Update global state pointer
    currentDirectoryIndex = idx;
}

static void cmd_touch(const char *name) {
    if (!name || !*name) {
			printf("Error: Missing filename.\n"); 
			return; 
		}

    if (find_node_in_current_dir(name) != -1) {
			printf("Error: Name already exists here.\n"); 
			return; 
		}

    int slot = find_free_node_slot();
    if (slot < 0) {
			printf("Error: Node table full.\n");
			return; 
		}

    // Try to allocate physical disk space
    int startBlk = allocate_disk_blocks(DEFAULT_FILE_ALLOC);
    if (startBlk < 0) {
			printf("Error: Virtual Disk full or fragmented.\n");
			return; 
		}

    // Set up file data
    NodeTable[slot].inUse = true;
    NodeTable[slot].isDirectory = false; // It's a file
    strncpy(NodeTable[slot].name, name, MAX_NAME_LEN - 1);
    NodeTable[slot].name[MAX_NAME_LEN - 1] = '\0';
    NodeTable[slot].parentIndex = currentDirectoryIndex; 

    NodeTable[slot].isOpen = false;
    NodeTable[slot].startBlockPtr = startBlk;
    NodeTable[slot].blockCount = DEFAULT_FILE_ALLOC;
    NodeTable[slot].sizeBytes = 0;

    printf("File '%s' created (Allocated %dKB).\n", NodeTable[slot].name, (DEFAULT_FILE_ALLOC * BLOCK_SIZE) / 1024);
}

static void cmd_open(const char *name) {
    if (!name) {
			printf("Usage: open <name>\n");
			return; 
		}

    int idx = find_node_in_current_dir(name);
    if (idx < 0 || NodeTable[idx].isDirectory) {
			printf("Error: File not found.\n");
			return; 
		}

    if (NodeTable[idx].isOpen) {
			printf("Warning: File is already open.\n");
		} else {
			NodeTable[idx].isOpen = true;
			printf("File '%s' is now OPEN.\n", NodeTable[idx].name); 
		}
}

static void cmd_close(const char *name) {
    if (!name) {
			printf("Usage: close <name>\n");
			return; 
		}

    int idx = find_node_in_current_dir(name);
    if (idx < 0 || NodeTable[idx].isDirectory) {
			printf("Error: File not found.\n");
			return; 
		}

    if (!NodeTable[idx].isOpen) {
			printf("Warning: File was not open.\n");
		} else {
			NodeTable[idx].isOpen = false;
			printf("File '%s' CLOSED.\n", NodeTable[idx].name); 
		}
}

static void cmd_write(const char *name) {
    if (!name) {
			printf("Usage: write <name>\n");
			return; 
		}

    int idx = find_node_in_current_dir(name);
    if (idx < 0) { 
			printf("Error: File not found.\n");
			return; 
		}

    if (NodeTable[idx].isDirectory) {
			printf("Error: Cannot write text to a directory.\n");
			return; 
		}

    if (!NodeTable[idx].isOpen) {
			printf("Error: File is closed. Use 'open' first.\n");
			return; 
		}

    int maxCapacity = NodeTable[idx].blockCount * BLOCK_SIZE;
    char inputBuffer[INPUT_BUFFER];

    printf("Enter text data (will overwrite existing data):\n>> ");
    if (!fgets(inputBuffer, sizeof(inputBuffer), stdin)) {
			return;
		}

    trim_newline(inputBuffer);

    int dataLen = (int)strlen(inputBuffer);
    if (dataLen >= maxCapacity) {
        printf("Warning: Input truncated to fit allocated blocks.\n");
        dataLen = maxCapacity - 1;
    }

    // Calculate physical address in vDisk array
    unsigned char* diskAddress = &vDisk[NodeTable[idx].startBlockPtr * BLOCK_SIZE];
    
    // Perform the simulated write
    memcpy(diskAddress, inputBuffer, dataLen);
    diskAddress[dataLen] = '\0'; // Null terminate on disk for safety
    NodeTable[idx].sizeBytes = dataLen;

    printf("Success: Wrote %d bytes.\n", dataLen);
}

static void cmd_read(const char *name) {
    if (!name) {
			printf("Usage: read <name>\n");
			return; 
		}
		
    int idx = find_node_in_current_dir(name);
    if (idx < 0) {
			printf("Error: File not found.\n");
			return; 
		}
    if (NodeTable[idx].isDirectory) {
			printf("Error: Cannot read directory as text. Use 'ls'.\n");
			return; 
		}

    if (!NodeTable[idx].isOpen) {
			printf("Error: File is closed. Use 'open' first.\n");
			return; 
		}

    if (NodeTable[idx].sizeBytes == 0) {
			printf("(File is empty)\n");
			return; 
		}

    printf("--- File Beginning: '%s' ---\n", NodeTable[idx].name);
    // Calculate physical address and print to stdout
    unsigned char* diskAddress = &vDisk[NodeTable[idx].startBlockPtr * BLOCK_SIZE];
    fwrite(diskAddress, 1, NodeTable[idx].sizeBytes, stdout);
    printf("\n--- File Ending: ---\n");
}

/* Main program loop */
int main(void) {
    init_filesystem();

    printf("\nRAM File System Simulation\n");
    printf("Type 'help' for instructions\n\n");

    char lineBuffer[INPUT_BUFFER];
    while (1) {
        // Print prompt showing current path
        printf("[fileSystem:");
        print_path_recursive(currentDirectoryIndex);
        printf("] > ");

        if (!fgets(lineBuffer, sizeof(lineBuffer), stdin)) {
					break;
				}

        trim_newline(lineBuffer);
        if (lineBuffer[0] == '\0') {
					continue;
				}

        // Tokenize input
        char *cmd = strtok(lineBuffer, " \t");
        char *arg = strtok(NULL, "");

        // Clean up leading spaces in argument if present
        if (arg) {
					while (*arg==' '||*arg=='\t') {
						arg++; 
						if (*arg=='\0') {
							arg=NULL; 
						}
					}
				}

        // Command dispatch
        if (strcmp(cmd, "help") == 0) {
					cmd_help();

				} else if (strcmp(cmd, "ls") == 0) {
					cmd_ls();

				} else if (strcmp(cmd, "mkdir") == 0) {
					cmd_mkdir(arg);

				} else if (strcmp(cmd, "cd") == 0) {
					cmd_cd(arg);

				} else if (strcmp(cmd, "touch") == 0) {
					cmd_touch(arg);

				} else if (strcmp(cmd, "open") == 0) {
					cmd_open(arg);

				} else if (strcmp(cmd, "write") == 0) {
					cmd_write(arg);

				} else if (strcmp(cmd, "close") == 0) {
					cmd_close(arg);

				} else if (strcmp(cmd, "read") == 0) {
					cmd_read(arg);

				} else if (strcmp(cmd, "exit") == 0 || strcmp(cmd,"quit") == 0) {
					break;

				} else { 
					printf("Unknown command: '%s'. Type 'help'.\n", cmd);

				}
    }

    printf("Simulation done.\n");
    return 0;
}
