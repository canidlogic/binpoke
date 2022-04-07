/*
 * binpoke.c
 * =========
 * 
 * See the README.md for further information.
 */

#define AKS_TRANSLATE_MAIN
#include "aksmacro.h"

#include "aksview.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* On Windows, make sure we are building in Unicode */
#ifdef AKS_WIN
#ifndef AKS_WIN_WAPI
#error binpoke: UNICODE and _UNICODE must be defined for Windows builds!
#endif
#ifndef AKS_WIN_WCRT
#error binpoke: UNICODE and _UNICODE must be defined for Windows builds!
#endif
#endif

/*
 * Local data
 * ==========
 */

/*
 * The executable module name, used in diagnostic messages.
 * 
 * Set at the start of the program entrypoint.
 */
const char *pModule = NULL;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void fault(int line);
static void printInt64(int64_t v);

static int verb_list(
    const char *pPath,
    const char *pFrom,
    const char *pFor);

static int verb_read(
    const char *pPath,
    const char *pAt,
    const char *pAs);

static int verb_write(
    const char *pPath,
    const char *pAt,
    const char *pAs,
    const char *pWith);

static int verb_query(const char *pPath);
static int verb_resize(const char *pPath, const char *pWith);
static int verb_require(const char *pPath);
static int verb_new(const char *pPath);

/*
 * Invoked when there is an unexpected error condition.
 * 
 * Prints a diagnostic message to stderr and then exits program with
 * failure code.
 * 
 * Parameters:
 * 
 *   line - the line number of the fault in this source file
 * 
 * Return:
 * 
 *   this function never returns
 */
static void fault(int line) {
  fprintf(stderr, "Fault in binpoke at line %d\n", line);
  exit(EXIT_FAILURE);
}

/*
 * Print a signed 64-bit integer value in decimal to standard output.
 * 
 * Parameters:
 * 
 *   v - the integer value to print
 */
static void printInt64(int64_t v) {
  
  /* Handle the different cases */
  if (v <= INT64_MIN) {
    /* Special case of the least negative value -- this integer value
     * has no positive equivalent due to the way two's complement works,
     * so just print its value as a special case */
    printf("-9223372036854775808");
    
  } else if ((v > INT64_MIN) && (v < 0)) {
    /* If we have a negative value besides the least negative value that
     * we just handled as a special case, print a negative sign and then
     * recursively print the positive value */
    putchar('-');
    printInt64(0 - v);
    
  } else if (v >= 10) {
    /* If value is positive and has more than one digit, first
     * recursively print the value except for the last digit */
    printInt64(v / 10);
    
    /* Finally, print the last digit */
    putchar(((int) (v % 10)) + '0');
    
  } else if ((v >= 0) && (v < 10)) {
    /* If value is positive and has one digit, just print the digit */
    putchar(((int) v) + '0');
    
  } else {
    /* Shouldn't happen */
    fault(__LINE__);
  }
}

/*
 * @@TODO:
 */
static int verb_list(
    const char *pPath,
    const char *pFrom,
    const char *pFor) {
  fprintf(stderr, "verb_list %s from %s for %s\n", pPath, pFrom, pFor);
  return 1;
}

/*
 * @@TODO:
 */
static int verb_read(
    const char *pPath,
    const char *pAt,
    const char *pAs) {
  fprintf(stderr, "verb_read %s at %s as %s\n", pPath, pAt, pAs);
  return 1;
}

/*
 * @@TODO:
 */
static int verb_write(
    const char *pPath,
    const char *pAt,
    const char *pAs,
    const char *pWith) {
  fprintf(stderr, "verb_write %s at %s as %s with %s\n",
    pPath, pAt, pAs, pWith);
  return 1;
}

/*
 * Verb to report the file size of an existing file.
 * 
 * Parameters:
 * 
 *   pPath - the path to the file
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int verb_query(const char *pPath) {
  
  int status = 1;
  int errcode = 0;
  int64_t fl = 0;
  AKSVIEW *pv = NULL;
  
  /* Check parameter */
  if (pPath == NULL) {
    fault(__LINE__);
  }
  
  /* Open a read-only view */
  pv = aksview_create(pPath, AKSVIEW_READONLY, &errcode);
  if (pv == NULL) {
    status = 0;
    fprintf(stderr, "%s: Failed to open file: %s\n",
              pModule, aksview_errstr(errcode));
  }
  
  /* Print the file length */
  if (status) {
    printf("File length: ");
    printInt64(aksview_getlen(pv));
    printf("\n");
  }
  
  /* Close viewer if open */
  aksview_close(pv);
  
  /* Return status */
  return status;
}

/*
 * @@TODO:
 */
static int verb_resize(const char *pPath, const char *pWith) {
  fprintf(stderr, "verb_resize %s with %s\n", pPath, pWith);
  return 1;
}

/*
 * Verb to create a new, empty file or do nothing if file already
 * exists.
 * 
 * Parameters:
 * 
 *   pPath - the path to the file
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int verb_require(const char *pPath) {
  
  int status = 1;
  int errcode = 0;
  AKSVIEW *pv = NULL;
  
  /* Check parameter */
  if (pPath == NULL) {
    fault(__LINE__);
  }
  
  /* Open a view in regular mode to create a new file if one doesn't
   * exist otherwise open existing file */
  pv = aksview_create(pPath, AKSVIEW_REGULAR, &errcode);
  if (pv == NULL) {
    status = 0;
    fprintf(stderr, "%s: Failed to open file: %s\n",
              pModule, aksview_errstr(errcode));
  }
  
  /* Close viewer if open */
  aksview_close(pv);
  
  /* Return status */
  return status;
}

/*
 * Verb to create a new, empty file.
 * 
 * Parameters:
 * 
 *   pPath - the path to the file
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int verb_new(const char *pPath) {
  
  int status = 1;
  int errcode = 0;
  AKSVIEW *pv = NULL;
  
  /* Check parameter */
  if (pPath == NULL) {
    fault(__LINE__);
  }
  
  /* Open a view in exclusive mode to create a new file without
   * overwriting any existing file */
  pv = aksview_create(pPath, AKSVIEW_EXCLUSIVE, &errcode);
  if (pv == NULL) {
    status = 0;
    fprintf(stderr, "%s: Failed to open file: %s\n",
              pModule, aksview_errstr(errcode));
  }
  
  /* Close viewer if open */
  aksview_close(pv);
  
  /* Return status */
  return status;
}

/*
 * Program entrypoint
 * ==================
 * 
 * This is the "translated" entrypoint.  The actual entrypoint is
 * defined by aksmacro.h
 */

static int maint(int argc, char *argv[]) {
  
  int status = 1;
  int x = 0;
  
  const char *pVerb = NULL;
  const char *pPath = NULL;
  const char *pFrom = NULL;
  const char *pFor  = NULL;
  const char *pAt   = NULL;
  const char *pAs   = NULL;
  const char *pWith = NULL;
  
  /* Get the module name */
  pModule = NULL;
  if ((argc > 0) && (argv != NULL)) {
    pModule = argv[0];
  }
  if (pModule == NULL) {
    pModule = "binpoke";
  }
  
  /* Make sure arguments are present */
  if (argc > 0) {
    if (argv == NULL) {
      fault(__LINE__);
    }
    for(x = 0; x < argc; x++) {
      if (argv[x] == NULL) {
        fault(__LINE__);
      }
    }
  }
  
  /* If nothing beyond the module name is passed, just print the
   * invocation syntax summary and fail */
  if (argc < 2) {
    status = 0;
    fprintf(stderr,
      "binpoke syntax summary:\n"
      "\n"
      "binpoke list [path] from [addr] for [count]\n"
      "binpoke read [path] at [addr] as [type]\n"
      "binpoke write [path] at [addr] as [type] with [value]\n"
      "binpoke query [path]\n"
      "binpoke resize [path] with [count]\n"
      "binpoke require [path]\n"
      "binpoke new [path]\n"
      "\n"
      "See the README for further documentation.\n");
  }
  
  /* Must have at least two extra parameters, containing the verb and
   * the binary file path */
  if (status && (argc < 3)) {
    status = 0;
    fprintf(stderr, "%s: Invalid invocation syntax!\n", pModule);
  }
  
  /* Must have an odd number of total arguments, since we have the
   * module name and then pairs of arguments after that */
  if (status && ((argc & 0x1) == 0)) {
    status = 0;
    fprintf(stderr, "%s: Invalid invocation syntax!\n", pModule);
  }
  
  /* Get the verb and file path */
  if (status) {
    pVerb = argv[1];
    pPath = argv[2];
  }
  
  /* For any arguments beyond the verb and file path, parse them as
   * phrases consisting of a preposition followed by a nominal; we
   * already checked earlier that the total number of arguments is odd
   * so we can step through extra arguments in pairs */
  if (status) {
    for(x = 3; x < argc; x += 2) {
      /* Handle each preposition, storing value in appropriate variable
       * and making sure the preposition hasn't been used before */
      if (strcmp(argv[x], "from") == 0) {
        if (pFrom == NULL) {
          pFrom = argv[x + 1];
        } else {
          status = 0;
          fprintf(stderr, "%s: Preposition used more than once: %s\n",
                    pModule, argv[x]);
        }
        
      } else if (strcmp(argv[x], "for") == 0) {
        if (pFor == NULL) {
          pFor = argv[x + 1];
        } else {
          status = 0;
          fprintf(stderr, "%s: Preposition used more than once: %s\n",
                    pModule, argv[x]);
        }
        
      } else if (strcmp(argv[x], "at") == 0) {
        if (pAt == NULL) {
          pAt = argv[x + 1];
        } else {
          status = 0;
          fprintf(stderr, "%s: Preposition used more than once: %s\n",
                    pModule, argv[x]);
        }
        
      } else if (strcmp(argv[x], "as") == 0) {
        if (pAs == NULL) {
          pAs = argv[x + 1];
        } else {
          status = 0;
          fprintf(stderr, "%s: Preposition used more than once: %s\n",
                    pModule, argv[x]);
        }
        
      } else if (strcmp(argv[x], "with") == 0) {
        if (pWith == NULL) {
          pWith = argv[x + 1];
        } else {
          status = 0;
          fprintf(stderr, "%s: Preposition used more than once: %s\n",
                    pModule, argv[x]);
        }
        
      } else {
        status = 0;
        fprintf(stderr, "%s: Unrecognized preposition: %s\n",
                  pModule, argv[x]);
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Dispatch to the appropriate verb function, checking that
   * appropriate preposition phrases have been provided for each */
  if (status) {
    if (strcmp(pVerb, "list") == 0) {
      if ((pFrom != NULL) &&
          (pFor  != NULL) &&
          (pAt   == NULL) &&
          (pAs   == NULL) &&
          (pWith == NULL)) {
        if (!verb_list(pPath, pFrom, pFor)) {
          status = 0;
        }
        
      } else {
        status = 0;
        fprintf(stderr, "%s: Wrong prepositional phrases for verb %s\n",
                  pModule, pVerb);
      }
      
    } else if (strcmp(pVerb, "read") == 0) {
      if ((pFrom == NULL) &&
          (pFor  == NULL) &&
          (pAt   != NULL) &&
          (pAs   != NULL) &&
          (pWith == NULL)) {
        if (!verb_read(pPath, pAt, pAs)) {
          status = 0;
        }
        
      } else {
        status = 0;
        fprintf(stderr, "%s: Wrong prepositional phrases for verb %s\n",
                  pModule, pVerb);
      }
      
    } else if (strcmp(pVerb, "write") == 0) {
      if ((pFrom == NULL) &&
          (pFor  == NULL) &&
          (pAt   != NULL) &&
          (pAs   != NULL) &&
          (pWith != NULL)) {
        if (!verb_write(pPath, pAt, pAs, pWith)) {
          status = 0;
        }
        
      } else {
        status = 0;
        fprintf(stderr, "%s: Wrong prepositional phrases for verb %s\n",
                  pModule, pVerb);
      }
      
    } else if (strcmp(pVerb, "query") == 0) {
      if ((pFrom == NULL) &&
          (pFor  == NULL) &&
          (pAt   == NULL) &&
          (pAs   == NULL) &&
          (pWith == NULL)) {
        if (!verb_query(pPath)) {
          status = 0;
        }
        
      } else {
        status = 0;
        fprintf(stderr, "%s: Wrong prepositional phrases for verb %s\n",
                  pModule, pVerb);
      }
      
    } else if (strcmp(pVerb, "resize") == 0) {
      if ((pFrom == NULL) &&
          (pFor  == NULL) &&
          (pAt   == NULL) &&
          (pAs   == NULL) &&
          (pWith != NULL)) {
        if (!verb_resize(pPath, pWith)) {
          status = 0;
        }
        
      } else {
        status = 0;
        fprintf(stderr, "%s: Wrong prepositional phrases for verb %s\n",
                  pModule, pVerb);
      }
      
    } else if (strcmp(pVerb, "require") == 0) {
      if ((pFrom == NULL) &&
          (pFor  == NULL) &&
          (pAt   == NULL) &&
          (pAs   == NULL) &&
          (pWith == NULL)) {
        if (!verb_require(pPath)) {
          status = 0;
        }
        
      } else {
        status = 0;
        fprintf(stderr, "%s: Wrong prepositional phrases for verb %s\n",
                  pModule, pVerb);
      }
      
    } else if (strcmp(pVerb, "new") == 0) {
      if ((pFrom == NULL) &&
          (pFor  == NULL) &&
          (pAt   == NULL) &&
          (pAs   == NULL) &&
          (pWith == NULL)) {
        if (!verb_new(pPath)) {
          status = 0;
        }
        
      } else {
        status = 0;
        fprintf(stderr, "%s: Wrong prepositional phrases for verb %s\n",
                  pModule, pVerb);
      }
      
    } else {
      status = 0;
      fprintf(stderr, "%s: Unrecognized verb: %s\n", pModule, pVerb);
    }
  }
  
  /* Determine return code and return */
  if (status) {
    status = EXIT_SUCCESS;
  } else {
    status = EXIT_FAILURE;
  }
  return status;
}
