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
 * Constants
 * =========
 */

/*
 * The maximum number of bytes that may be listed.
 * 
 * This is just to prevent a gigantic listing.  It is set here to 64K.
 */
#define LIST_MAXBYTES (INT64_C(65536))

/*
 * Type declarations
 * =================
 */

/*
 * Structure that stores a single line of data for a listing.
 */
typedef struct {
  
  /*
   * The paragraph number to display.
   * 
   * This is the 32 least significant bytes of the address, divided by
   * 16.
   */
  int32_t para;
  
  /*
   * The unsigned byte values to display.
   * 
   * This array has one element for each byte of the paragraph.
   * 
   * Elements that are present must be in range [0, 255].
   * 
   * You can also use negative values for elements that are not present,
   * such as occurs if you have a partial line that goes outside the
   * boundaries of the range being listed.
   */
  int bv[16];
  
} LIST_LINE;

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
static void printListLine(const LIST_LINE *pl);

static int64_t parseCount(const char *pstr);
static int parseHex(const char *pstr, uint64_t *pv);
static int64_t parseAddress(const char *pstr);

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
 * Print the contents of a listing line structure to standard output.
 * 
 * Parameters:
 * 
 *   pl - the structure to print
 */
static void printListLine(const LIST_LINE *pl) {
  
  int i = 0;
  
  /* Check parameter */
  if (pl == NULL) {
    fault(__LINE__);
  }
  
  /* Check paragraph number and print address field */
  if ((pl->para < 0) || (pl->para > INT32_C(0xfffffff))) {
    fault(__LINE__);
  }
  printf("%07lx0:", (long) pl->para);
  
  /* Print each byte value */
  for(i = 0; i < 16; i++) {
    /* Space is separator, except before element 8 there are three
     * spaces */
    if (i == 8) {
      printf("   ");
    } else {
      putchar(' ');
    }
    
    /* Range check byte value and print */
    if (((pl->bv)[i] >= 0) && ((pl->bv)[i] <= 255)) {
      /* Unsigned byte value */
      printf("%02x", (pl->bv)[i]);
      
    } else if ((pl->bv)[i] < 0) {
      /* Missing byte value */
      printf("  ");
      
    } else {
      /* Invalid byte value */
      fault(__LINE__);
    }
  }
  
  /* Print separator between bytes and characters */
  printf(" | ");
  
  /* Print each character value */
  for(i = 0; i < 16; i++) {
    /* Range check byte value and print */
    if (((pl->bv)[i] >= 0x20) && ((pl->bv)[i] <= 0x7e)) {
      /* US-ASCII printing character */
      putchar((pl->bv)[i]);
      
    } else if ((pl->bv)[i] < 0) {
      /* Missing byte value */
      putchar(' ');
      
    } else {
      /* Neither missing nor US-ASCII */
      putchar('.');
    }
  }
  
  /* Finally, print the line break */
  printf("\n");
}

/*
 * Parse a 64-bit count parameter from a given string.
 * 
 * These strings are unsigned decimal strings that must be in range of
 * int64_t.  -1 is returned if there is a parsing error.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 * Return:
 * 
 *   the unsigned count value, or -1 if parsing error
 */
static int64_t parseCount(const char *pstr) {
  
  int64_t result = 0;
  
  /* Check parameter */
  if (pstr == NULL) {
    fault(__LINE__);
  }
  
  /* Fail if empty string */
  if (*pstr == 0) {
    result = -1;
  }
  
  /* Skip any leading zeros */
  if (result >= 0) {
    for( ; *pstr == '0'; pstr++);
  }
  
  /* Parse any remaining digits */
  if (result >= 0) {
    for( ; *pstr != 0; pstr++) {
      /* Check that digit is valid */
      if ((*pstr < '0') || (*pstr > '9')) {
        result = -1;
      }
      
      /* Multiply result by ten, failing if overflow */
      if (result >= 0) {
        if (result <= INT64_MAX / 10) {
          result *= 10;
        } else {
          result = -1;
        }
      }
      
      /* Add new digit into result, failing if overflow */
      if (result >= 0) {
        if (result <= INT64_MAX - (*pstr - '0')) {
          result += (*pstr - '0');
        } else {
          result = -1;
        }
      }
      
      /* Leave loop if error */
      if (result < 0) {
        break;
      }
    }
  }
  
  /* Return result or -1 */
  return result;
}

/*
 * Parse a sequence of base-16 digits as an unsigned value.
 * 
 * pstr must point to a sequence of at least one base-16 character.
 * 
 * The result will be stored in *pv.  If parsing fails, an undefined
 * value will be in *pv.  Check the return to determine whether parsing
 * succeeded or not.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 *   pv - pointer to variable to receive parsed result
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int parseHex(const char *pstr, uint64_t *pv) {
  
  int status = 1;
  int d = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (pv == NULL)) {
    fault(__LINE__);
  }
  
  /* Reset result to zero */
  *pv = 0;
  
  /* Fail if empty string */
  if (*pstr == 0) {
    status = 0;
  }
  
  /* Parse all digits */
  if (status) {
    for( ; *pstr != 0; pstr++) {
      /* Get current digit value */
      if ((*pstr >= '0') && (*pstr <= '9')) {
        d = *pstr - '0';
      } else if ((*pstr >= 'a') && (*pstr <= 'f')) {
        d = *pstr - 'a' + 10;
      } else if ((*pstr >= 'A') && (*pstr <= 'F')) {
        d = *pstr - 'A' + 10;
      } else {
        status = 0;
      }
      
      /* Multiply result by 16, watching for overflow */
      if (status) {
        if (*pv <= UINT64_MAX / 16) {
          *pv *= 16;
        } else {
          status = 0;
        }
      }
      
      /* Add digit into result, watching for overflow */
      if (status) {
        if (*pv <= UINT64_MAX - d) {
          *pv += d;
        } else {
          status = 0;
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Parse an address encoded within a given string.
 * 
 * If the given string does not begin with "0x" or "0X" then this call
 * is equivalent to parseCount().
 * 
 * If the given string begins with "0x" or "0X" then parseHex() is
 * called on the rest of the string after the prefix.  If the unsigned
 * value returned from that is within 64-bit signed range, then that is
 * the result; otherwise, the function returns a failure code of -1.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 * Return:
 * 
 *   the parsed address, or -1 if parsing error
 */
static int64_t parseAddress(const char *pstr) {
  
  int64_t result = 0;
  uint64_t uv = 0;
  
  /* Check parameter */
  if (pstr == NULL) {
    fault(__LINE__);
  }
  
  /* Handle cases */
  if (strlen(pstr) < 2) {
    /* Less than two characters in string, so no possible way there
     * could be a prefix; call through to parseCount() */
    result = parseCount(pstr);
    
  } else if ((pstr[0] != '0') ||
                ((pstr[1] != 'x') && (pstr[1] != 'X'))) {
    /* Does not begin with a prefix, so call through to parseCount() */
    result = parseCount(pstr);
    
  } else {
    /* If we got here, we have a prefix, so parse everything after the
     * prefix as a hex string */
    if (parseHex(pstr + 2, &uv)) {
      /* We now have the unsigned 64-bit value; check whether in
       * range */
      if (uv <= INT64_MAX) {
        /* In range, so cast to signed to get result */
        result = (int64_t) uv;
        
      } else {
        /* Not in range, so parsing failed */
        result = -1;
      }
      
    } else {
      /* Parsing failed */
      result = -1;
    }
  }
  
  /* Return result or -1 */
  return result;
}

/*
 * Verb to generate a hex dump listing.
 * 
 * Parameters:
 * 
 *   pPath - path to the file
 * 
 *   pFrom - string parameter with starting address
 * 
 *   pFor - string parameter with byte count
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int verb_list(
    const char *pPath,
    const char *pFrom,
    const char *pFor) {
  
  int status = 1;
  int errcode = 0;
  AKSVIEW *pv = NULL;
  
  int64_t addr = 0;
  int64_t count = 0;
  
  int i = 0;
  int64_t p = 0;
  int64_t p_first = 0;
  int64_t p_last = 0;
  
  LIST_LINE ls;
  
  /* Initialize structures */
  memset(&ls, 0, sizeof(LIST_LINE));
  
  /* Check parameters */
  if ((pPath == NULL) || (pFrom == NULL) || (pFor == NULL)) {
    fault(__LINE__);
  }
  
  /* Get the address */
  addr = parseAddress(pFrom);
  if (addr < 0) {
    status = 0;
    fprintf(stderr, "%s: Failed to parse address: %s\n",
              pModule, pFrom);
  }
  
  /* Get the count */
  if (status) {
    count = parseCount(pFor);
    if (count < 0) {
      status = 0;
      fprintf(stderr, "%s: Failed to parse count: %s\n",
                pModule, pFor);
    }
  }
  
  /* Check that count is at least one and does not exceed limit */
  if (status && (count < 1)) {
    status = 0;
    fprintf(stderr, "%s: Count may not be less than one!\n", pModule);
  }
  
  if (status && (count > LIST_MAXBYTES)) {
    status = 0;
    fprintf(stderr, "%s: Count exceeds LIST_MAXBYTES limit!\n",
              pModule);
  }
  
  /* Open a read-only view */
  if (status) {
    pv = aksview_create(pPath, AKSVIEW_READONLY, &errcode);
    if (pv == NULL) {
      status = 0;
      fprintf(stderr, "%s: Failed to open file: %s\n",
                pModule, aksview_errstr(errcode));
    }
  }
  
  /* Check that address is within file limits */
  if (status) {
    if (addr >= aksview_getlen(pv)) {
      status = 0;
      fprintf(stderr, "%s: Given address is outside file limits!\n",
                pModule);
    }
  }
  
  /* Check that address added to count does not exceed file length */
  if (status) {
    if (addr + count > aksview_getlen(pv)) {
      status = 0;
      fprintf(stderr, "%s: Given byte range goes beyond end of file!\n",
                pModule);
    }
  }
  
  /* Compute the first and last paragraph addresses that will be
   * displayed in the listing */
  if (status) {
    p_first = addr / 16;
    p_last  = (addr + count - 1) / 16;
    
    p_first *= 16;
    p_last  *= 16;
  }
  
  /* Print a listing of each paragraph */
  if (status) {
    for(p = p_first; p <= p_last; p += 16) {
    
      /* Write the paragraph number into the structure */
      ls.para = (int32_t) ((p & INT64_C(0xffffffff)) / 16);
      
      /* Read each relevant byte, filling -1 for bytes outside the
       * requested range */
      for(i = 0; i < 16; i++) {
        if (((p + ((int64_t) i)) >= addr) &&
              ((p + ((int64_t) i)) < addr + count)) {
          /* Byte is in range */
          (ls.bv)[i] = (int) aksview_read8u(pv, p + ((int64_t) i));
          
        } else {
          /* Byte is not in range */
          (ls.bv)[i] = -1;
        }
      }
    
      /* Print the listing line */
      printListLine(&ls);
    }
  }
  
  /* Close viewer if open */
  aksview_close(pv);
  
  /* Return status */
  return status;
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
 * Verb to resize an existing file.
 * 
 * Parameters:
 * 
 *   pPath - the path to the file
 * 
 *   pWith - string parameter containing the new length
 */
static int verb_resize(const char *pPath, const char *pWith) {
  
  int status = 1;
  int errcode = 0;
  int64_t fl = 0;
  AKSVIEW *pv = NULL;
  
  /* Check parameters */
  if ((pPath == NULL) || (pWith == NULL)) {
    fault(__LINE__);
  }
  
  /* Parse the desired new file length */
  fl = parseCount(pWith);
  if (fl < 0) {
    status = 0;
    fprintf(stderr, "%s: Failed to parse count: %s\n",
              pModule, pWith);
  }
  
  /* Make sure file length doesn't exceed aksview limit */
  if (status && (fl > AKSVIEW_MAXLEN)) {
    status = 0;
    fprintf(stderr, "%s: Length exceeded AKSVIEW_MAXLEN!\n", pModule);
  }
  
  /* Open a read-write view */
  if (status) {
    pv = aksview_create(pPath, AKSVIEW_EXISTING, &errcode);
    if (pv == NULL) {
      status = 0;
      fprintf(stderr, "%s: Failed to open file: %s\n",
                pModule, aksview_errstr(errcode));
    }
  }
  
  /* Set the file length */
  if (status) {
    if (!aksview_setlen(pv, fl)) {
      status = 0;
      fprintf(stderr, "%s: Failed to set length on file!\n", pModule);
    }
  }
  
  /* Close viewer if open */
  aksview_close(pv);
  
  /* Return status */
  return status;
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
