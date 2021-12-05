#include "stdio.h"
#include "string.h"
#include "ctype.h"
#define MAXLEN 4096
#define MAXPGH 128

/*
   Intermediate Voynich Transliteration Tool.
   Processes files in the IVTFF format.
   by R.Zandbergen. ver 1.1, 10 April 2020.
*/

/* Global variables */
/* The following capture the information from the command line options. */
/* The specified defaults all imply doing nothing */

int a_high=0;    /* High Ascii left as is */
int kcomh=0;     /* Keep hash comment lines */
int kcomi=0;     /* Keep inline comments */
int s_hard=0;    /* Do not touch dot spaces */
int s_uncn=0;    /* Do not touch comma spaces */
int unr=0;       /* Keep unreadable characters: ?  (prev: * ) */
int brack=0;     /* Do not modify uncertain reading brackets */
int kfoli=0;     /* Keep foliation info */
int folign=0;    /* Do not ignore locus format (to support old files) */
int gaps=0;      /* Leave <-> sign (previously: - ) */
int para=0;      /* Leave <$> sign (previously: = ) and <%> sign */
int toppar=0;    /* Keep all normal paragraph text lines (P locus) */
int liga=0;      /* Keep ligature indication as is */
int white=0;     /* Leave white space */
int wrap=0;      /* Maintain line wrapping as it is in the file */
int wwidth;      /* New line wrapping limit */
int mute=0;      /* Normal output to stderr */
int infarg= -1;  /* Argument of input file name */
int oufarg= -1;  /* Argument of output file name */
char auth=' ';   /* Name of transliterator */
char uloc2=' ';  /* Second char of selected locus, if appl. */
int authrm = 0;  /* Do not remove the transliterator ID */
char invar[27];  /* List of 'include page' options */
char exvar[27];  /* List of 'exclude page' options */ 
int npgopt = 0;  /* Number of page include/exclude options (0 to 26) */
int nlcopt = 0;  /* Number of locus include/exclude options (0 or 1)*/
FILE *fin, *fout; /* File handles */

/* Some file stats */
int nlpart = 0   /* Number of (partial) lines read from stdin */;
int nlread = 0   /* Number of lines from stdin after unwrapping */;
int nldrop = 0   /* Number of lines dropped */;
int nlhash = 0   /* Number of hash lines suppressed */;
int nlempt = 0   /* Number of empty lines suppressed */;
int nlwrit = 0   /* Number of lines for output */;
int nlwrap = 0   /* Number of wrapped lines written to output*/;

/* These give info about a complete line, set in GetLine and/or PrepLine */
int comlin=0        /* 1 if line starts with # - set in GetLine */;
int filehead=0      /* ==2 if line 1 and a valid header - set in GetLine*/;
int hastrtxt=0      /* 1 if there is transliterated text - set in GetLine */;
int hasfoli=0       /* 1 if there are < > starting in the first position */;
int newpage=0       /* 1 if there are < > without a period (i.e. a new page) */;
int nwpar=0         /* 1 if the line includes a <%> code */;
char cwarn=' '      /* Character  for which a warning is issued */;
char folname[] = "      " ;  /* *folname= "      "   Folio name, e.g. f85r3 */
int num = 0;        /* The locus number */
char lineauth= ' ';          /* Name of transliterator or blank */
char cator = ' ';            /* the locator character */
char loc2[] = "   ";         /* the 2-character locus type */
char pgvar[27]               /* List of page variable settings */;
char txtag[27]               /* List of text tag settings */;
int hastag          /* 1 if a page header indicates the use of text tags */;

/* These track the text */
int in_comm=0;    /* not 0 if inside < > comment */
int in_foli=0;    /* not 0 if inside locus < > */
int ind_ligo= -1; /* Position of ligatures { char */
int ind_ligc= -1; /* Position of ligatures } char */
int ind_alto= -1; /* Position of alt. readings [ char */
int ind_altb= 0;  /* Position of first alt. readings : char */
int ind_altc= -1; /* Position of alt. readings ] char */
int word0 = -1;   /* Position of first char of a word */
int word1 = -1;   /* Position of last char of a word */
int weir0 = -1;   /* Position of 'weirdo' dollar (obsolescent) */
int hasc0 = -1;   /* Position of high ascii starter (@) */
int hasc1 = -1;   /* Position of high ascii  semicolon */
int dedcom = 0;   /* Not 0 if this is a 3-char dedicated comment */
int tagcom = 0;   /* Not 0 if this is a 6-char text tag comment */
char comchr = ' '; /* The character defining the type of inl.comment */
int indtag = 0;   /* Text tag index */

/* Further global variables for certain options */
int concat = 0;   /* In GetLine: used for judging CR and spaces */
int highasc = 0;  /* In PrepLine: Ascii code of @...;  */
int pend_hd = 0;  /* Set to 1 if a page header is waiting to be output */
char cue = '.';   /* The 'space' after which wrapping is allowed */
char pgh[MAXPGH];

/*-----------------------------------------------------------*/

void shiftl(char *b,int index,int nrlost)

/* Shift left by (nrlost) bytes the part of string (b)
   after (index) */

/*char *b; 
  int index, nrlost;*/

{
  int ii;
  for (ii = index+1; ii<MAXLEN-nrlost; ii++) {
    b[ii] = b[ii+nrlost];
  }
  return;
}

/*-----------------------------------------------------------*/

int FindSpace(char *buf,int width)
/* Locate rightmost hard space character in buf[0 .. width-1].
   Return index, or zero if buf is shorter, or <0 if none
   found */

/*char *buf; 
  int width;*/

{
  int result = -1, ii = 0, voycode = 1;
  char cb;

  while ((cb = buf[ii]) && ii < width) {
    /* Dedicated check for comments.
       Sets voycode if outside comments */
    if (cb == '<') voycode = 0;
    else if (cb == '>') voycode = 1;
    else { 
      if (cb == cue || cue == (char) 0) {
        if (voycode) result = ii;
      }  
    }
    ii += 1;
  }
  if (cb == 0) result = 0;
  return result;
}

/*-----------------------------------------------------------*/
        
void showvar(char code,char *vars,int head)
/*char code, *vars;
int head;*/

/* Print all page variables , only whle debugging */

{
  int i;
  if (head) { /* Print header legend */
    fprintf(stderr, "%c ", code);
    for (i=0; i<=26; i++) fprintf(stderr, "%c", i+64);
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "%c ", code);
  for (i=0; i<=26; i++) fprintf(stderr, "%c", vars[i]);
  fprintf(stderr, "\n");
}

/*-----------------------------------------------------------*/

void clearvar()

/* Reset all page variables and text tags */

{
  int i;
  for (i=0; i<=26; i++) {
     pgvar[i]=' ';
     txtag[i]='@';
  }
  hastag = 0;
  /*
    fprintf(stderr,"clearing variables...\n");
    showvar('P',pgvar,1);
  */
}

/*-----------------------------------------------------------*/

int usepgloc(int iopt)

/* This used to be called 'usepage'
   If called with iopt == 1 (once per page)
   check the list of pgvar[] (only) with include and
   exclude options given by the user.

   If called with iopt == 0 (once per locus), do two things:

   Check the list of pgvar[] and txtag[] with include and
   exclude options given by the user.
   Then use page var. <at> to select 
   loci, either as single or dual-character locus types.
   Also check for paragraph start lines if the locus is P

   The value of pgvar[0] was set in GetLine.
   The values of txtag[] were set in Track (invoked by PrepLine)
*/

{
  int i, select, mat;
  char usevar;
  
  select = 1;
  
  /* fprintf(stderr,"usepgloc called, option %d\n",iopt); */
  if (iopt) {
  
    /* Here it is called for the whole page. Only look at the page
       variables. If a variable is set to @, ignore it  */
    hastag = 0;
    for (i=1; i<=26; i++) {
      usevar = pgvar[i];
      if (invar[i] != ' ') {
        if (usevar == '@') {
          hastag = 1;
        } else {
          if (invar[i] != usevar) select=0;
        }
      }
      if (exvar[i] != ' ') {
        if (usevar == '@') {
          hastag = 1;
        } else {
          if (exvar[i] == usevar) select=0;
        }
      }
    }
    /*  showvar('P',pgvar,1); 
        showvar('I',invar,0);
        showvar('X',exvar,0);
    */ 
  } else {

    /* Here it is called for a locus (or the file header) */
    if (filehead) return 1;

    /* First check the combination of page variables and text tags. */
    for (i=1; i<=26; i++) {
      /* decide whether to go by page var. or by text tag */
      usevar = pgvar[i];
      if (usevar == '@') {
        usevar = txtag[i];
        /* fprintf(stderr,"Checking text tag %d : %c\n",i,usevar); */
      } else {
        if (usevar != ' ') {
          /* fprintf(stderr,"Checking page var %d : %c\n",i,usevar); */
        }
      }
      if (usevar == '@') usevar = ' ';

      if (invar[i] != ' ') {
        if (invar[i] != usevar) select=0;
        /* fprintf(stderr,"Incl.ind: %d , val: %c , result: %d\n",i,invar[i],select); */
      }
      if (exvar[i] != ' ') {
        if (exvar[i] == usevar) select=0;
      }
    }

    /* Only if it is selected, then also check the 
       locus type, 1 or 2 char */
    if (select) {
      if (invar[0] != ' ') {
        mat = (invar[0] != loc2[0]);
        if (uloc2) {
          mat = (mat || (uloc2 != loc2[1]));
        }
        if (mat) select=0;
      }
      if (exvar[0] != ' ') {
        mat = (exvar[0] == loc2[0]);
        if (uloc2) {
          mat = (mat && (uloc2 == loc2[1]));
        }
        if (mat) select=0;
      }
    }

    /* Now handle processing related to -q option */
    if (select && toppar) {
      if (loc2[0] == 'P') {
        if (toppar == 1 && nwpar == 0) select = 0;
        if (toppar == 2 && nwpar) select = 0;
      }
    }
  
  }  

/* fprintf (stderr,"select(%d): %d\n",iopt,select); */

  return select;
}

/*-----------------------------------------------------------*/

void trackinit()

/* Initialise tracker */

{
    in_comm = 0;   /* >0 if inside < > comment */
    dedcom = 0;    /* >0 if a dedicated comment */
    tagcom = 0;    /* >0 if a text tag comment */
    in_foli = 0;   /* >0 if inside locus < > */
    ind_ligo = -1; /* Position of ligatures { char */
    ind_ligc = -1; /* Position of ligatures } char */
    ind_alto = -1; /* Position of alt. readings [ char */
    ind_altb = 0;  /* Position of alt. readings : char */
    ind_altc = -1; /* Position of alt. readings ] char */
    word0 = - 1;   /* Position of first char of a word */
    word1 = - 1;   /* Position of last char of a word */
    comchr = ' ';  /* First character of a comment */
}

/*-----------------------------------------------------------*/

int trackerr(char *buf,char cget)

/* Print track error */

/*char *buf, cget;*/
{
  if (mute < 2) {
    fprintf(stderr, "Offending character: %c\n", cget);
    fprintf(stderr, "Line parsed so far: %s\n", buf);
    return 1;
  }
}

/*-----------------------------------------------------------*/

int Track(char cb,int index)
/* Keep track of comments, foliation, ligatures, etc */
/* Return 0 if OK, 1 if error */
/* Will only be called for line without # comment */

/*char cb;
  int index;*/
{

  /* Evolve previous reading of bracket */
  if (in_comm == -1) in_comm = 1;
  if (in_comm == -2) in_comm = 0;
  if (in_foli == -1) in_foli = 1;
  if (in_foli == -2) in_foli = 0;
  
  /* If a word was just finished, start a new one */
  if (word1 >= 0) {
    word0 = -1; word1 = -1;
  }
  
  /* Check for in-line comments.
     Error if inside foliation brackets < >,
     warning if inside [ ] or { } */

  if (cb == '<' && index > 0) {
    if (in_foli != 0 ) {
      if (mute < 2) fprintf(stderr,"%s\n","Illegal bracket inside locus");
      return 1;
    }
    if (ind_ligo > ind_ligc || ind_alto > ind_altc) cwarn='<';
    if (in_comm != 0) { /* Already open */
      if (mute < 2) fprintf(stderr,"%s\n","Illegal second open bracket");      
      return 1;
    }
    if (word0 >= 0 && word1 <0) word1 = index-1; /* End word here */ 
    in_comm = -1; comchr = ' ';
    return 0;
  }

  /* Check end of an in-line or dedicated comment */
  if (cb == '>') {
    if (in_comm > 0) {
      if (dedcom) {
        if (in_comm > 2) {
          if (mute < 2) fprintf(stderr,"%s\n","Dedicated comment >1 character");      
          return 1;
        }
        dedcom = 0;
        /* Check for first line of paragraph */
        if (comchr == '%') {
          nwpar = 1;
        }
      }
      if (tagcom) {
        if (in_comm > 5) {
          if (mute < 2) fprintf(stderr,"%s\n","Text tag >4 characters");      
          return 1;
        }
        tagcom = 0; indtag = 0;
      }
      word0 = -1; word1 = -1; /* Prepare for new word */
      in_comm = -2; return 0;
    }
  }

  /* New < > comment, decode its type */
  if (in_comm == 1) {
    comchr = cb;
    tagcom = (comchr == '@');
    if (tagcom == 0) dedcom = (comchr != '!');

    /* if (dedcom) fprintf(stderr,"%s\n","Dedicated commment found");
       if (tagcom) fprintf(stderr,"%s\n","Text tag found"); */
  }

  /* Process text tag */
  if (tagcom) {
    if (in_comm == 2) {
      if (cb < 'A' || cb > 'Z') {
        cwarn = '@';
        if (mute < 2) fprintf(stderr,"%s\n","Illegal text tag");    
        return 1;
      } else {
        indtag = cb - 64;
      }
      /* fprintf(stderr,"Found text tag index %d\n",indtag); */
    }
    if (in_comm == 4) {
      if (indtag) txtag[indtag] = cb;
      /* fprintf(stderr,"Found text tag value %c\n",cb); */
    }
  }

  /* Continue checks only outside < > comments */
  if (in_comm != 0) {
    if (in_comm > 0) in_comm += 1;
    return 0;
  }

  /* Check for foliation. It is already guaranteed that nothing
     else is open */
  if (cb == '<' && index == 0) {
    in_foli = -1; return 0;
  }

  /* A > now has to be the end of foliation */
  if (cb == '>') {
    if (in_foli == 0) {
      /* It was not */
      cwarn = '>';
      if (mute < 2) fprintf(stderr,"%s\n","Illegal close bracket:");    
      return 1;
    }
    if (in_foli < 3) { /* Should really be longer */
      if (mute < 2) fprintf(stderr, "%s\n", "Foliation field too short");
      return 1;
    }
    in_foli = -2; return 0;
  }

  /* Check for alternate reading brackets */
  if (cb == '[') {
    if (in_foli != 0) { /* Not allowed inside < > */
      cwarn = '[';
      if (mute < 2) fprintf(stderr, "%s\n", "Illegal bracket inside foliation ");
      return 1;
    }
    if (ind_alto > ind_altc) { /* Unclosed bracket already open */
      cwarn = '[';
      if (mute < 2) fprintf(stderr, "%s\n", "Illegal second open bracket: [");
      return 1;
    }
    ind_alto = index; ind_altb = -1; ind_altc = -1;
  } else if ((cb == '|') || (cb == ':')) {
    /* Support old files where this is a separator. However, outside [ ]
       it is always a legal character */
    if (ind_alto > 0) {
      /* Here a [ bracket was already open */
      if (ind_altb < 0) ind_altb = index;   /* Keep only left-most */
    }
  } else if (cb == ']') {
    if (ind_alto <= 0) { /* No bracket open */
      cwarn = ']';       
      if (mute < 2) fprintf(stderr, "%s\n", "Illegal close bracket");
      return 1;
    }
    if (ind_altb < 0) { /* Separator : sign missing, issue warning */ 
      cwarn = ':';       
      ind_altb = ind_alto + 2; /* To continue reasonably */
    }
    ind_altc = index;
  }

  /* Check for ligature brackets, not allowed inside < > */

  if (cb == '{') {
    if (in_foli != 0) { /* not allowed inside < > */
      cwarn = '{';
      if (mute < 2) fprintf(stderr, "%s\n", "Illegal bracket inside locus < >");
      return 1;
    }
    if (ind_ligo > ind_ligc) { /* Unclosed paren already open */
      cwarn = '{';
      if (mute < 2) fprintf(stderr, "%s\n", "Illegal second open bracket");
      return 1;
    }
    ind_ligo = index; ind_ligc = -1;
    return 0;
  }
  if (cb == '}') {
    if (ind_ligo < 0) {
      cwarn = '}';
      if (mute < 2) fprintf(stderr,"%s\n","Illegal close bracket");
      return 1; /* No bracket open */
    }
    ind_ligc = index;
    return 0;
  }

  /* Inside foliation keep track of index */
  if (in_foli > 0) in_foli += 1;
  
  /* Further checks of word boundaries, not in foliation */
  if (in_foli == 0) {
  
    if (cb == ',' || cb == '.' || cb == ' ' ||
        cb == '\n' || cb == '-') { /* Word delimiter */
      if (word0 >= 0) word1 = index - 1;
    } else { /* non-space char */
      if (word0 < 0) word0 = index;
    }

  }
  
  return 0;
}

/*-----------------------------------------------------------*/

void TrackLight(char cb,int index)
/* Same as track, but do not do error checking */
/* Will only be called for line without # comment */

/*char cb;
int index;*/
{

  /* Evolve previous reading of bracket */
  if (in_comm == -1) in_comm = 1;
  if (in_comm == -2) in_comm = 0;
  if (in_foli == -1) in_foli = 1;
  if (in_foli == -2) in_foli = 0;
  
  /* If a word was just finished, start a new one */
  if (word1 >= 0) {
    word0 = -1; word1 = -1;
  }
  
  /* Check for in-line comments. */
  if (cb == '<' && index > 0) {
   if (word0 >= 0 && word1 < 0) word1 = index - 1; /* End word here */ 
    in_comm = -1; return;
  }
  if (cb == '>' && in_foli == 0) {
    word0 = -1; word1 = -1; /* Prepare for new word */
    in_comm = -2; return;
  }

  /* Check end of an in-line comment */
  if (cb == '>') {
    if (in_comm > 0) {
      word0 = -1; word1 = -1; /* Prepare for new word */
      in_comm = -2; return;
    }
  }

  /* Decode type of < > comment */
  if (in_comm == 1) {
    comchr = cb;
    tagcom = (comchr == '@');
    if (tagcom == 0) dedcom = (comchr != '!');
  }

  /* Continue only outside < > comments */
  if (in_comm != 0) {
    if (in_comm > 0) in_comm += 1;
    return;
  }

  /* Check for foliation */
  if (cb == '<' && index == 0) {
    in_foli = -1; return;
  }
  if (cb == '>' && in_foli > 0) {
    in_foli = -2; return;
  }

  /* Check for alternate reading brackets */
  if (cb == '[') {
    ind_alto = index; ind_altb = -1; ind_altc = -1;
  } else if (cb == ':') {
    if (ind_altb < 0) ind_altb = index; /* Keep only left-most */
  } else if (cb == ']') {
    ind_altc = index;
  }

  /* Check for ligature brackets */

  if (cb == '{') {
    ind_ligo = index; ind_ligc = -1;
    return;
  }
  if (cb == '}') {
    ind_ligc = index;
    return;
  }

  /* Inside foliation keep track of index */
  if (in_foli > 0) {
    in_foli += 1; 
  }
  
  /* Further checks of word boundaries, not in foliation */
  if (in_foli == 0) {
  
    if (cb == ',' || cb == '.' || cb == ' ' ||
        cb == '\n' || cb == '-') { /* Word delimiter */
      if (word0 >= 0) word1 = index - 1;
    } else { /* non-space char */
      if (word0 < 0) word0 = index;
    }

  }
  
  return;
}

/*-----------------------------------------------------------*/

void OutChar(char cb)

/* Write character cb to Ascii file */

/*char cb; */

{
  /* Print the character itself */
  fputc (cb, fout);
  
  /* If character just written was newline: */
  if (cb == '\n') {
    /* Every newline implies an extra 'wrapped line' */
    nlwrap += 1;
  }
  
  return;
}

/*-----------------------------------------------------------*/

void OutString(char *buf)

/* Write string of characters to Ascii file */

/*char *buf;*/ 

{
  int index = 0;
  char cb;
  
  while (cb = buf[index]) {
/*  if (comlin == 0) {
      TrackLight(cb, index);
    }                                  */
    OutChar(buf[index++]);
    /* Complete lines are identified by a newline
       passed via routine OutString */ 
    if (cb == '\n') nlwrit += 1;
  }
  return;
}

/*-----------------------------------------------------------*/

int ParseOpts(int argc,char *argv[])
/* Parse command line options. There can be many and each should be
   of one of the following types:
   -pv with lower case p: one of:
     a,b,c,f,h,l,m,p,q,s,u,w,x where v can be 0-9, plus optionally -tC where 
     C is the transliterator code. This can also be +tC.
   -Pc or +Pc with upper case P: page variable where c is A-Z or
     0-9. If P=<at> then used for locus type, c=P for normal loci.
   <filename>:
     Maximum two, where first is input file name and second is
     output file name  */
/*int argc;
  char *argv[];*/
{
  int i;
  char sign, optn, val, val2;

  for (i=0; i<=26; i++) {
    invar[i]=' '; exvar[i]=' ';
  }

  /* Suppress uninformative message until further notice */
  /* if (mute == 0) fprintf (stderr,"Parsing Command Line Options\n"); */

  for (i=1; i<argc; i++) {
    sign=argv[i][0];
    if (sign == '-' || sign == '+') {
    
      /* Normal option, not file name */
      
      optn=argv[i][1]; val=argv[i][2];
      if (val) {
        /* Get any additional character, usually a null */
        val2 =argv[i][3];
      }

      /* process each one */

      if (optn >= '@' && optn <= 'Z') {

        /* Upper case: */
        if (sign == '+') {
          invar[optn-64]=val;
        } else {
          exvar[optn-64]=val;
        }
        /* One special case */
        if (optn == '@') {
          uloc2 = val2; /* NULL or char */
          kcomh = 1;   /* By default, suppress # comment lines */
        }
      } else {
      
        /* Lower case: */
        switch (optn) {
           case 'a':
             if (val == '0') a_high = 0; /* High ascii as is */
             else if (val == '1') a_high = 1;  /* High ascii to @123; */
             else if (val == '2') a_high = 2;  /* @123; to high ascii */
             break;
           case 'b':
             /* this no longer looks at % and ! placeholders */
             if (val == '0') {
               white = 0;               /* Keep blanks */
             }
             else if (val == '1') {
               white = 1;               /* Strip blanks */
             }
             break;
           case 'c':
             if (val == '0') { kcomh=0; kcomi=0; } /* Keep comments */
             else if (val == '1') { kcomh=1; kcomi=0; }/* Strip # */
             else if (val == '2') { kcomh=0; kcomi=1; }/* Strip <! > */
             else if (val == '3') { kcomh=0; kcomi=2; }/* Strip all */
             else if (val == '4') { kcomh=1; kcomi=1; }/* Strip all */
             else if (val == '5') { kcomh=1; kcomi=2; }/* Strip all */
             break;
           case 'f':
             if (val == '0') kfoli=0; /* Keep foliation info */
             else if (val == '1') kfoli=1; /* Strip it */
             else if (val == '9') folign=1; /* Ignore locus layout */
                          /* but still process page headers */
             break;
           case 'm':
             if (val == '0') {
               mute=0; /* All output to stderr */
             } else if (val == '1') {
               mute=1; /* suppress warnings */
             } else if (val == '2') {
               mute=2; /* suppress everything */
             }
             /* The setting of this option is not reported */
             break;
           case 'p':
             if (val == '0') {
               gaps=0; para=0; /* Keep <-> and <%> , <$> */
             } else if (val == '1') {
               gaps=1; para=1; /* Gaps and <%> , <$> stripped */
             } else if (val == '2') {
               gaps=2; para=2; /* Gaps to extra space and <$> to extra NL */
             }
             break;
           case 'q':
             if (val == '0') {
               toppar=0; /* Keep all P locus lines */
             } else if (val == '1') {
               toppar=1; /* Keep only top P locus lines */
               para=1; /* and remove paragraph markers */
             } else if (val == '2') {
               toppar=2; /* Delete all top P locus lines */
               para=1; /* and remove paragraph markers */
             }
             break;
           case 'l':
             if (val == '0')  liga=0;     /* Leave ligature brackets */
             else if (val == '1') liga=1; /* Strip them */
             else if (val == '2') liga=2; /* This option will be ignored*/
             else if (val == '3') liga=3; /* Change to capitalisation rule */
             else if (val == '4') liga=4; /* As 3, but change cth to CTh.. */
             break;
           case 'h':
             if (val == '0') s_uncn = 0;      /* Keep comma for uncertain space*/
             else if (val == '1') s_uncn = 1; /* Treat comma as dot */
             else if (val == '2') s_uncn = 2; /* Strip uncertain spaces */
             else if (val == '3') {
               s_uncn = 3;                    /* Turn uncertain spaces to ? */
               unr = 1;                       /* And turn words with ? into ??? */
             }
             break;
           case 's':
             if (val == '0') {
               s_hard = 0; cue = '.';      /* Keep dot for hard space*/
             } else if (val == '1') {
               s_hard = 1; cue = ' ';      /* Turn hard space to blank */
             } else if (val == '2') {
               s_hard = 2; cue = (char) 0; /* Strip hard space */
               s_uncn = 2;        /* And also uncertain spaces of course */
             } else if (val == '3') {
               s_hard = 3; cue = '.';     /* Convert space to newline */
               kfoli = 1;         /* For this also remove foliation ... */
               wrap = 1;         /* ... and force line unwrapping */
             }
             break;
           case 't':
             auth = val;  /* This also used to strip placeholders */
             /* New:  +tX keeps transliterator code, but -tX removes it */
             if (sign == '-') authrm = 1;
             break;
           case 'u':
             if (val == '0') {
               unr=0; brack=0; /* leave ? and [] as is */
             } else if (val == '1') {
               unr=0; brack=1; /* Take first of [] */
             } else if (val == '2') {
               unr=0; brack=2; /* Turn [] to ? */
             } else if (val == '3') {
               unr=1; brack=2; /* Turn word with * into ??? */
             } else if (val == '4') {
               unr=2; brack=2; /* Remove ? or word with * */
             } else if (val == '5') {
               unr=3; brack=2; /* Remove line with ? * or [..] */}
             break;
           case 'w':
             /* Completely ignore option if spaces converted to line breaks */
             if (s_hard != 3) {
               if (val == '0') wrap=0;       /* Maintain line wrapping */
               else if (val == '1') wrap=1;  /* Unwrap all continuation lines */
               else {
                 wrap=2;                     /* Re-wrap */
                 wwidth = 20 * (val - '0');
               }
             } 
             break;
           case 'x':
             if (val == '0') {
               kcomh = 1; kcomi = 0; gaps = 1; para = 1;
               wrap = 1; white = 1; 
             } else if (val == '1') {
               kcomh = 1; kcomi = 2; kfoli = 1; gaps = 1;
               para = 1; white = 1; 
             } else if (val == '2') {
               kcomh = 1; kcomi = 2; kfoli = 1; gaps = 1;
               para = 1; white = 1; brack = 1;
             } else if (val == '3') {
               s_hard = 1; s_uncn = 1; cue = ' ';
             } else if (val == '4') {
               s_hard = 1; s_uncn = 2; cue = ' ';
             } else if (val == '5') {
               liga = 1; a_high = 2;
             } else if (val == '6') {
               liga = 4; a_high = 2;
             } else if (val == '7') {
               kcomh = 1; kcomi = 2; kfoli = 1; brack = 1; liga = 1; 
               white = 1; gaps = 1; para = 1;
               s_hard = 1; cue = ' '; s_uncn = 1;
             } else if (val == '8') {
               kcomh = 1; kcomi = 2; kfoli = 1; brack = 1; liga = 1; 
               white = 1; gaps = 1; para = 1;
               s_hard = 1; cue = ' '; s_uncn = 2;
             }
             break;
        }
      }
    } else {
      /* A file name. Check which of two */
      if (infarg < 0) {
        infarg = i;
      } else if (oufarg < 0) {
        oufarg = i;
      } else {
        if (mute < 2) fprintf (stderr, "%s\n", "Only two file names allowed");
        return 1;
      }
    }
  }
  return 0;
}

/*-----------------------------------------------------------*/

int DumpOpts(int argc,char *argv[])
/* Print information on selected options
   and open input and output files if required */
/*int argc;
char *argv[];*/
  {
  int i;
  if (mute == 0) {
    fprintf (stderr,"%s\n","Summary of options:");

    /* Process each one */
    switch (a_high) {
       case 0: fprintf (stderr,"%s\n","Leave high ascii as is");
           break;
       case 1: fprintf (stderr,"%s\n","Expand high ascii to @...; notation");
           break;
       case 2: fprintf (stderr,"%s\n","Convert @...; notation to 1 byte");
           break;
    }
    switch (kcomh) {
       case 0: fprintf (stderr,"%s\n","Keep hash comment lines");
           break;
       case 1: fprintf (stderr,"%s\n","Remove hash comment lines");
           break;
    }
    switch (kcomi) {
       case 0: fprintf (stderr,"%s\n","Keep all inline comments");
           break;
       case 1: fprintf (stderr,"%s\n","Remove inline comments except page headers");
           break;
       case 2: fprintf (stderr,"%s\n","Remove inline comments, page headers and text tags");
           break;
    }
    switch (kfoli) {
       case 0: fprintf (stderr,"%s\n","Keep foliation");
           break;
       case 1: fprintf (stderr,"%s\n","Remove foliation and file header");
           break;
    }
    switch (folign) {
       case 1: fprintf (stderr,"%s\n","Ignore locus format (parse old file)");
           break;
    }
    switch (liga) {
       case 0: fprintf (stderr,"%s\n","Keep ligature brackets");
           break;
       case 1: fprintf (stderr,"%s\n","Remove ligature brackets");
           break;
       case 2: fprintf (stderr,"%s\n","Invalid option, ignored");
           liga = 0;
           break;
       case 3: fprintf (stderr,"%s\n","Change ligature brackets to capitalisation");
            break;     
       case 4: fprintf (stderr,"%s\n","Use extended EVA capitalisation");
            break;
    }
    switch (s_uncn) {
       case 0: fprintf (stderr,"%s\n","Use comma for uncertain spaces");
           break;
       case 1: fprintf (stderr,"%s\n","Treat uncertain spaces as normal spaces (see below)");
           break;
       case 2: fprintf (stderr,"%s\n","Remove uncertain spaces");
           break;
       case 3: fprintf (stderr,"%s\n","Turn words next to uncertain spaces to ?");
           break;
    }
    switch (s_hard) {
       case 0: fprintf (stderr,"%s\n","Use dot for normal spaces");
           break;
       case 1: fprintf (stderr,"%s\n","Use space for normal spaces");
           break;
       case 2: fprintf (stderr,"%s\n","Remove normal spaces");
           break;
       case 3: fprintf (stderr,"%s\n","Convert normal spaces to line breaks");
           break;
    }
    switch (white) {
       case 0: fprintf (stderr,"%s\n","Keep white space");
           break;
       case 1: fprintf (stderr,"%s\n","Remove white space");
           break;
    }
    switch (brack) {
       case 0: fprintf (stderr,"%s\n","Keep alternate readings notation");
           break;
       case 1: fprintf (stderr,"%s\n","Take first of alternate readings");
           break;
       case 2: fprintf (stderr,"%s\n","Turn alternate readings into ?");
           break;
    }
    switch (unr) {
       case 0: fprintf (stderr,"%s\n","Keep words containing ?");
           break;
       case 1: fprintf (stderr,"%s\n","Turn words containing ? into ???");
           break;
       case 2: fprintf (stderr,"%s\n","Remove words containing ?");
           break;
       case 3: fprintf (stderr,"%s\n","Remove lines containing ?");
           break;
    }
    switch (para) {
       case 0: fprintf (stderr,"%s\n","Keep paragraph start / end code");
           break;
       case 1: fprintf (stderr,"%s\n","Remove paragraph start / end code");
           break;
       case 2: fprintf (stderr,"%s\n","Replace para end code by extra newline");
           break;
    }
    switch (gaps) {
       case 0: fprintf (stderr,"%s\n","Keep drawing intrusion code");
           break;
       case 1: fprintf (stderr,"%s\n","Remove drawing intrusion code");
           break;
       case 2: fprintf (stderr,"%s\n","Change drawing intrusion code into extra space");
           break;
    }
    switch (wrap) {
       case 0: fprintf (stderr,"%s\n","Maintain line wrapping");
           break;
       case 1: fprintf (stderr,"%s\n","Unwrap continuation lines");
           break;
       default: fprintf (stderr,"(Re)wrap lines at %3d \n", wwidth);
           break;
    }

    /* Locus handling */
    fprintf (stderr,"\nLine selection options:\n");
    if (auth == ' ') {
      fprintf (stderr,"%s\n","Ignore transliterator ID");
    } else {
      fprintf (stderr,"%s %c\n","Use only data from transliterator",auth);
      if (authrm) {
        fprintf (stderr,"%s\n","Transliterator ID will be removed");
      } else {
        fprintf (stderr,"%s\n","Transliterator ID will be kept");
      }
    }
    if (invar[0] != ' ') {
      if (uloc2) {
        fprintf (stderr,"Include only locus type %c%c\n", invar[0], uloc2);
      } else {
        fprintf (stderr,"Include only locus type %c\n", invar[0]);
      }
      nlcopt = 1;
    }
    if (exvar[0] != ' ') {
      if (uloc2) {
        fprintf (stderr,"Exclude only locus type %c%c\n", invar[0], uloc2);
      } else {
        fprintf (stderr,"Exclude locus type %c\n", exvar[0]);
      }
      nlcopt = 1;
    }
    switch (toppar) {
       case 0: fprintf (stderr,"%s\n","Keep all normal paragraph text lines");
           break;
       case 1: fprintf (stderr,"%s\n","Keep only first normal paragraph text lines");
           break;
       case 2: fprintf (stderr,"%s\n","Keep all but first normal paragraph text lines");
           break;
    }
  
    /* Page splitter */
    fprintf (stderr,"\nPage/tag selection options (exclude overrules include):\n");

    for (i=1; i<=26; i++) {
      if (invar[i] != ' ') {
        fprintf (stderr, "- Include if variable %c set to %c\n", i+64, invar[i]);
        npgopt += 1;
      }
      if (exvar[i] != ' ') {
        fprintf (stderr, "- Exclude if variable %c set to %c\n", i+64, exvar[i]);
        npgopt += 1;
      }
    }
    if (npgopt == 0) fprintf (stderr, "- Include all.\n");

  }  /* End of: if (mute == 0) */

  /* Files */
  if (mute == 0) fprintf (stderr,"\nInput file: ");
  if (infarg >= 0) {
    if (mute == 0) fprintf(stderr, "%s\n",argv[infarg]);
    if ((fin = fopen(argv[infarg], "r")) == NULL) {
      if (mute < 2) fprintf(stderr, "Input file does not exist\n");
      return 1;
    }
  } else {
    fin = stdin;
    if (mute == 0) fprintf (stderr,"<stdin>\n");
  }
  
  if (mute == 0) fprintf (stderr,"Output file: ");
  if (oufarg >= 0) {
    if (mute == 0) fprintf(stderr, "%s\n", argv[oufarg]);
    if ((fout = fopen(argv[oufarg], "w")) == NULL) {
      if (mute < 2) fprintf(stderr, "Cannot open output file\n");
      return 1;
    }
  } else {
    fout = stdout;
    if (mute == 0) fprintf (stderr, "<stdout>\n");
  } 
  return 0;
}

/*-----------------------------------------------------------*/

int GetLine(char *buf)
/* Get line from stdin to buffer */
/* Return 0 if all OK, <0 if EOF, 1 if error */
/* Concatenate lines ending in slash if wrap option >0 */
/* Avoid confusion with / locator in locus using ugly hack */

/*char *buf;*/
{
  int cr = 0, eod = 0, index = 0, iget, blank, ignore;
  int ii;
  char cget;
  char ctest[8] = "#=IVTFF ";

  /* Set these global parameters */
  comlin = 0; hastrtxt = 0; concat = 0; cator = ' '; loc2[0]='\0'; loc2[1]='\0';

  while (cr == 0 && eod == 0) {
    ignore = 0;
    iget = fgetc(fin);
    /* fprintf(stderr, "Index %3d,  char %3d\n", index, iget); */

    /* Check for end of file. Only allowed at first read */
    eod = (iget == EOF);
    if (eod) {
      if (index == 0) {
        return -1;       /* Correct EOF */
      } else {
        buf[index] = 0;  /* Add a null character for safety */
        if (mute < 2) {
          fprintf (stderr, "EOF at record pos. %3d\n", index);        
          fprintf (stderr, "Line read so far: %s\n", buf); 
        }
        return -2;
      }
    }

    cget = (char) iget;
    if (cget == ' ' || cget == '\t' || cget == '\n') {
      blank = 1;
    } else {
      blank = 0;
    }
     
    /* Check for comment line */

    if (cget == '#') {
      /* A hash symbol. Not allowed when unwrapping */
      if (concat > 0) {
        buf[index] = 0;  /* Add a null character for safety */
        if (mute < 2) {
          fprintf (stderr, "Hash comment after continuation\n");        
          fprintf (stderr, "Line read so far: %s\n", buf); 
        }
        return 1;
      }
      /* Check hash comment line */
      if (index == 0) {
        comlin = 1;
        /* Later on also check for file header */
      }
    }
    
    /* Unwrap-processing only if this is not a hash comment */
    if (comlin == 0 && wrap > 0) {
      if (concat == 0) { /* Search for slash */
        if (cget == '/') {  /* avoid confusion with / locator code */
          if (index > 11) {
            concat = 1;
            ignore = 1;
          }
        }
      } else if (concat == 1) { /* Search for newline */
        ignore = 1;
        if (cget == '\n') {
          concat = 2;
        } else if (blank == 0) {
          concat = 0;
          ignore = 0;
        }
      } else if (concat == 2) { /* Search for non-blank */
        if (blank == 0 && cget != '/') {
          /* This actualy skips any nr of / at the start, not just 1 */
          concat = 0;
          nlpart += 1; /* Only here count continuation */
        } else {
          ignore = 1;
        }        
      }
    }

    /* Process this char only if ignore not set */
    if (ignore == 0) {

      /* Should add the character to the buffer */
      /* However, first check for buffer overflow */
      if (index >= (MAXLEN-2)) {
        buf[index] = '\n';
        buf[index+1] = 0;
        if (mute < 2) {
          fprintf (stderr, "Record too long.\n");        
          fprintf (stderr, "Line read so far: %s\n", buf); 
        }
        return 1;
      }

      /* Now safe to add */
      buf[index++] = cget;
      if (blank == 0) hastrtxt = 1;
     
      /* Check for newline */
      if (cget == '\n') {
        cr = 1;
        buf[index] = 0; /* Add null at end of line */
      }
    } /* End if ignore == 0 */
  }

  /* Now initiase filehead based on the first line of the file.
     Value 2 if it is a complete header, 1, if it is a comment,
     or 0 if it is neither.
     After this it can only decrease */
  if (nlread == 0) {
    filehead = 2;
    for (ii=1; ii<=7; ii++) {
      if (buf[ii] != ctest[ii]) filehead = 1;
    }
    if (comlin == 0) filehead = 0;
    if (filehead != 2) {
      if (mute == 0) fprintf (stderr, "Input file has no IVTFF header.\n");        
    }
  } else {
    if (filehead == 2) filehead = 1;
    if (comlin == 0) filehead = 0;
  }
  return 0;
}

/*-----------------------------------------------------------*/

int PrepLine(char *buf1,char *buf2)
/* Preprocess line that was just read from file or stdin to buffer */
/* This completely decodes the locus ID information */
/* Return 0 if all OK, <0 if EOF, 1 if error */

/*char *buf1, *buf2;*/
{
  int ind1 = 0, ind2 = 0, varpt = 0;
  unsigned char cget, varid;

  int in_locu = 0;  /* Track locus ID (OBSOLESCENT) */
  int in_num = 0;   /* Track locus 'num' (new) */
  int in_loc = 0;   /* Track locator and locus type (new) */
  int in_auth = 0;  /* Track transliterator */
  int ignore;       /* Processing white space */
  int ii;           /* Used to convert char(ijk) to @ijk; */

  /* Set these global parameters */
  hasfoli = 0; newpage = 0; nwpar = 0;

  /* comlin and hastrtxt were already set in GetLine */
  
  trackinit();

  while (cget = buf1[ind1]) {
    
    /* For hash comment just copy: */
    if (comlin) {
      buf2[ind2++] = cget;
      /* fprintf(stderr, "Index %3d,  char %3d\n", ind2, cget); */
    } else {
      /* No comment line: do all the processing */
      
      /* 1. A hash later in the line is a legal character.  
            No need to do anything */
  
      /* 2. Check for white space, leaving it untouched inside
         inline comments */
      ignore = 0;
      if (cget == ' ' || cget == '\t') {
        if (in_comm == 0 && white == 1) ignore = 1;
      }

      /* 3. Process ligature brackets if desired */
      if (liga == 1) {
        if (cget == '{' || cget == '}' ) ignore = 1;
      }

      /* Process this char only if ignore not set */
      if (ignore == 0) {

        /* Check for open caret in 1st position */
        if (cget == '<' && in_comm == 0) {
          if (ind1 == 0) {
            hasfoli = 1;
          }
        }
  
        /* Track the text */
        if (Track(cget, ind1)) {
          /* Returned with an error. Add a null. */
          buf2[ind2] = 0;
          return trackerr(buf2, cget);
        }

        /* If necessary, replace old style alt.reading separator */
        if ((cget == '|') && (ind_altc < ind_alto)) cget = ':';

        /* Process Ascii(128-255) if desired */
        if (in_comm == 0 && a_high == 1) {
          if (cget > 127) {
            buf2[ind2++]= '@';  
            ii = cget / 100; buf2[ind2++] = ii + 48;
            cget -= (100 * ii); ii = cget / 10;  buf2[ind2++] = ii + 48;
            ii = cget - (10 * ii); buf2[ind2++] = ii + 48;
            /* buf2[ind2++] = ';'; */
            cget = ';'; ignore = 1;
          } 
        }
      
        /* Process @...; if desired */

        if (in_comm == 0 && a_high == 2) {
          if (hasc0 >= 0 && hasc1 < 0) {
            if (cget >= '0' && cget <= '9') {
              /* Process numbers between & and ; */
              highasc = 10 * highasc + (cget - '0');
              /* fprintf(stderr, "Hiasc(I): %3d\n", highasc); */
            }
          }
          if (cget == '@') {
            if (in_foli == 0) { /* Do not get confused by locator code */
              if (hasc0 >= 0) { /* Already open */
                if (mute < 2) fprintf(stderr, "Illegal %c after @ \n", cget);      
                return 1;
              }
              hasc0 = ind2;
              hasc1 = -1;
              highasc = 0;
            }
          }

          if (cget == ';') {
            if (in_foli == 0) { /* Do not get confused by transliterator code */
              if (hasc0 <= 0) { /* Not open */
                if (mute < 2) fprintf(stderr, "%s\n", "Illegal semi-colon");    
                return 1;
              }
              hasc1 = ind2; /* This will trigger code at end */
            }
          }
        }

        /* Process all locus details inside foliation brackets */
        if (in_foli != 0) {

          /* Evolve pointers */
          if (in_num == -1) in_num = 1;
          if (in_num == -2) in_num = 0;
          if (in_loc == -1) in_loc = 1;
          if (in_loc == -2) in_loc = 0;
          if (in_auth == -1) in_auth = 1;
          if (in_auth == -2) in_auth = 0;

          /* Check for < . , ; and > */
          /* However, most of this only if the foliation is meaningful.
             If folign is set, only look for < and > with no other
             puctuation in between */

          if (folign == 0) {
            /* This is the case where locus details are interpreted */
            switch (cget) {
              case '<':
                *folname = '\0';
                cator = ' ';
                *loc2 = '\0';
                lineauth = ' ';
                newpage = -1;
                break;
              case '>': /* Terminate (as needed) locus ID */
                if (newpage < 0) {
                  /* Not a new locus but a new page. Prepare for var. reading */
                  newpage = 1; clearvar();
                } else {
                  /* End of locus. */
                  /* Did it end with a transliterator ID? */
                  if (in_auth == 2) {
                    in_auth = -2;
                  } else {  /* must be end of locus type */
                    if (in_loc != 4) {
                      if (mute < 2) fprintf(stderr, "%s\n", "Illegal > after incomplete locus");
                      return 1;
                    }
                    /* Terminate locus ID */
                    in_loc = -2;
                  }
                }
                in_foli = -2;
                break;
              case '.': /* Start of locus number */
                if (in_num != 0) {  /* this test must be improved */
                  if (mute < 2) fprintf(stderr, "%s\n", "Illegal second . inside locus ID");
                  return 1;
                }
                /* Start of the locus number */
                newpage = 0;
                in_num = -1;
                num = 0;
                break;
              case ',': /* Start of locus type, with locator */
                if (in_num <= 0) { /* should have been decoding number */
                  if (mute < 2) fprintf(stderr, "%s\n", "Illegal , without locus num");
                  return 1;
                }
                in_num = -2;
                in_loc = -1;
                break;
              case ';': 
                if (in_loc != 4) {
                  if (mute < 2) fprintf(stderr, "%s\n", "Illegal ; after incomplete locus");
                  return 1;
                }
                /* Terminate locus ID */
                in_loc = -2;
                /* Start author ID */
                in_auth = -1;
                break;
              default:
                /* Any other char. Check to which field it belongs */
                /* Part of transliterator ID ? */
                if (in_auth > 1) {
                  if (mute < 2) fprintf(stderr, "%s\n", "Illegal author ID length");
                  return 1;
                }
                if (in_auth == 1) { /* Get author ID */
                  lineauth = cget;
                  in_auth = 2;
                  break;
                }  
                /* Part of locus ? */
                if (in_loc > 0) {
                  if (in_loc > 3) {
                    if (mute < 2) fprintf(stderr, "%s\n", "Locus type more than 3 char");
                    return 1;
                  }
                  if (in_loc > 1) {
                    loc2[in_loc-2] = cget;
                  } else {  /* The locator character */
                    cator = cget;
                  }
                  in_loc += 1;
                  break;
                }
                /* Part of number ? */
                if (in_num > 0) {
                  if (in_num > 3) {
                    if (mute < 2) fprintf(stderr, "%s\n", "Locus number more than 3 char");
                    return 1;
                  }
                  num = 10 * num + (cget - '0');
                  in_num += 1;
                  break;
                }
                /* If we are here it must be part of the page name */
                if (in_foli > 7) {
                  if (mute < 2) fprintf(stderr, "%s\n", "Page name more than 6 char");
                  return 1;
                } 
                folname[in_foli-1] = cget;
                folname[in_foli] = 0;

            } /* End Case (for folign == 0) */

          } else {
            /* This is the case where most locus details are ignored */
            switch (cget) {
              case '<':
                newpage = -1;
                break;
              case '>':
                if (newpage < 0) {
                  /* Not a new locus but a new page. Prepare for var. reading */
                  newpage = 1; clearvar();
                }
                in_foli = -2;
                break;
              case '.':
              case ',':
              case ';':
                newpage = 0;
                break;
              default:
                /* Nothing to be done */
                break;
            } /* End Case (for folign != 0) */

          } /* End of switch for folign */

        } /* End of: if in_foli != 0 */

        /* Check for page variables, only if start of new page found */
        if (newpage == 1) {

          if (varpt == -3) {
            if (cget < 'A' || cget > 'Z') {
              cwarn = '$'; varpt = 0;
            } else {
              varid = cget;
            }
          }

          if (varpt == -1) pgvar[varid-64] = cget;

          /* Advance pointer */
          if (varpt < 0) varpt += 1;

          /* Only check for a new $ when not already in sequence */
          if (varpt == 0) {
            if (cget == '$') {
              varid = ' '; varpt = -3;
            }
          } 
        } /* End if newpage == 1 */

        /* Add the character to the buffer */
        buf2[ind2++] = cget;
        /* Check for complete &..; code */
        if (a_high == 2) {
          if (hasc1 >= 0) {
            /* fprintf(stderr, "Hiasc(F): %3d\n", highasc); */
            buf2[hasc0] = (char) highasc;
            ind2 = hasc0 + 1;
            hasc0 = hasc1 = -1;
          }
        }  
      } /* End if ignore == 0 */
      
    } /* End of switch hash / no hash */
    ind1 += 1;
 
  } /* End of input string */
  buf2[ind2] = 0; /* Complete the output buffer */

  /* Now the entire line was read, do some more checks */

  if (comlin == 0) {
    if (in_comm) {
      if (mute < 2) fprintf(stderr, "%s", "Unclosed comment\n");
      return 1;
    }
    if (in_foli) {
      if (mute < 2) fprintf(stderr, "%s", "Unclosed foliation comment\n");
      return 1;
    }
    if (ind_alto > ind_altc) {
      if (mute < 2) fprintf(stderr, "%s", "Unclosed alternate reading\n");
      return 1;
    }
    if (ind_ligo > ind_ligc) {
      if (mute < 2) fprintf(stderr, "%s", "Unclosed ligature\n");
      return 1;
    }
    
    /* Set pgvar[0] if a new locus was read */
    /* May not be needed anymore */
    if (hasfoli && (newpage == 0)) {
      pgvar[0] = loc2[0];
    }
  }

  /* No transliterated text on page header, only needed if foliation removed */
  if (newpage && (kfoli == 1)) hastrtxt = 0;

  return 0;
}

/*-----------------------------------------------------------*/

int ProcRead(char *buf)
/* Process uncertain readings and/or ligature capitalisation rule.
   Read and write to same buffer
   Return 0 if all OK, -1 if line to be deleted,
   1 if error */
   
/*char *buf;*/
{
  int index, ii, ret=0, locq;

  char cb;

  /* Check if anything needs to be done at all */
  if (brack == 0 && unr == 0 && liga < 3) return ret;

  /* If it is empty, nothing either */
  if (hastrtxt == 0) return ret;

  /* If it is a hash line , nothing either */
  if (comlin != 0) return ret;

  /* First loop over line takes care of ligature
     matters and the [] brackets */
  /* Initialise a few things */
  trackinit(); index = 0;

  while (cb = buf[index]) {

    /* Track the text */
    TrackLight(cb, index);
    
    /* If extended capitalisation is used and the character is h */
    if (liga == 4 && cb == 'h' && index > 0) {
      /* Only if not inside {..} and not inside <! ..> */
      if (in_comm == 0 && ind_ligo <0) {
        switch (buf[index-1]) {
          case 's':
          case 't':
          case 'k':
          case 'p': 
          case 'f':
            buf[index-1] = toupper(buf[index-1]);
        }
      }
    }
     
    /* If a complete set of liga parens found: */
    if (ind_ligc >= 0) {
      if (liga >= 3) {

        /* Capitalise all but last char */
        index = ind_ligc - 2;
        for (ii=ind_ligo; ii<index; ii++) {
          buf[ii] = toupper(buf[ii+1]);
        }
        buf[index] = buf[index+1];
        /* If it ends with a quote, un-capitalise the one before */
        if (buf[index] == '\'') buf[index-1] = tolower(buf[index-1]);

        /* Shift left remainder of line */
        shiftl(buf, index, 2);
      }

      /* reset pointers */
      ind_ligo = -1; ind_ligc = -1;
    }

    /* If a complete set of alt.read brackets found: */
    if (ind_altc >= 0) {
      if (brack == 1) {

        /* Process into best guess: */
        index = ind_altb - 2;
        for (ii=ind_alto; ii<=index; ii++) {
          buf[ii] = buf[ii+1];
        }

      } else if (brack == 2) {

        /* Process into unreadable or
           prepare for processing into ??? */
        if (unr == 0) {
          index = ind_alto;
        } else if (unr == 1) {
          index = ind_altc;
        }
        buf[index] = '?'; 
      }

      /* Shift left remainder of line if needed */
      if (index < ind_altc) {
        shiftl(buf, index, ind_altc-index);
      }
      
      /* reset pointers */
      ind_alto= -1; ind_altb= 0; ind_altc= -1;
    }
    index += 1;
  }
  
  /* If necessary, second loop over line takes care
     of uncertain word readings */
  locq= -1; 
  if (unr == 0) return ret;
  
  /* Initialise tracker */
  trackinit(); index = 0;
  /* fprintf(stderr,"%s\n",buf); */
  while (cb = buf[index]) {

    /* Track the text */
    TrackLight(cb, index);

    /* Look for unreadable */
    if (cb == '?') locq = index; 

    if (word1 >= 0) {
      /* A complete word was read: check ? */
      /* fprintf(stderr,"word from %d to %d\n",word0,word1); */
      if (locq >= 0) {
        /* fprintf(stderr,"q:  %d\n",locq); */
        if (unr == 1) {
          /* Action: change word into ??? */
          index = word0;
          buf[index] = '?';
          if (word1 > index) {
            word0 += 1; index = word0; buf[index] = '?';
          }
          if (word1 > index) {
            word0 += 1; index = word0; buf[index] = '?';
          }
          /* Shift left remainder of line */
          if (word1 > word0) {
            shiftl(buf, index, word1-word0);
          }
          index += 1; word0 = -1; word1 = -1;
        
        } else  if (unr == 2) {
          
          /* Action: drop this word */
          index = word0 - 1;
          shiftl(buf, index, word1 - word0 + 1);
          index += 1; word0 = -1; word1 = -1;
        
        } else  if (unr == 3) {
          
          /* Action: drop this line. */  
          return -1;    
        }  
        /* fprintf(stderr,"%s\n\n",buf); */
        /* Reset pointers */
        locq = -1;  
      }        

    }
    index += 1;
  }
  return 0;
}

/*-----------------------------------------------------------*/

int ProcSpaces(char *buf1,char *buf2)
/* Process spaces in buffer. This treats occurrences of
   comma and dot (but not % !) in input text, but also
   dedicated comments related to drawing intrusions and end
   of paragraph.
   In addition, it removes text tags if necessary.
   Return 0 if all OK, 1 if error */
/*char *buf1, *buf2;*/
{
  int indin=0, indout=0, eol=0;
  int addchar;
  char cb, cbo;

  /* Some standard 'track' initialisations per line */
  trackinit();

  /* If no text, do little */
  if (hastrtxt == 0) {
    buf2[0] = 0;
    return 0;
  }

  while (eol == 0) {
    cb = buf1[indin];
    eol = (cb == (char) 0);

    if (comlin == 0) {
      /* Track the text */
      TrackLight(cb, indin);
    }

/*  if (in_comm == -1) fprintf(stderr,"%s","Start of comment\n");
    if (in_comm == -2) fprintf(stderr,"%s","End of comment\n");
    if (in_folio == -1) fprintf(stderr,"%s","Start of foliation\n");
    if (in_folio == -2) fprintf(stderr,"%s","End of foliation\n");  */

    indin += 1;
    addchar = 1;

    /* Check outside comments only */
    if (comlin == 0 && in_comm == 0 && in_foli == 0) {

      /* Check hard and uncertain spaces */
      cbo = cb;
      if (cb == ',') {
        if (s_uncn == 1) cb = cbo = '.';  /* Treat same as dot space*/
        else if (s_uncn == 2) addchar=0;  /* Convert to blank */
        else if (s_uncn == 3) cbo = '?';  /* Convert to unreadable */
      }
      if (cb == '.') {
        if (s_hard == 1) cbo = ' ';       /* Just use blank instead */
        else if (s_hard == 2) addchar=0;  /* Convert to nothing */
        else if (s_hard == 3) cbo = '\n';  /* Convert to line break */
      }
      cb = cbo;
      
    }

    while (addchar--) {
      buf2[indout++] = cb;
    }
  }
  return 0;
}

/*-----------------------------------------------------------*/

int PutLine(char *buf)
/* Write buffer to output, optionally skipping foliation info,
   all types of comments and a few other things.
   If foliation is suppressed, blank spaces before transliterated text
   will also not be output.
 */

/* Return 0 if all OK, 1 if error */

/*char *buf;*/
{
  int index=0, indout=0, eol=0, output, ii;
  int leftjust;
  int authch = 0;
  char cb;
  char cbo;
  char wrapbuf[MAXLEN];

  leftjust = 0;

  /* check if this is a hash comment line, but not the file header */
  if (comlin != 0 && filehead != 2) {
    /*  Print immediately or exit  */
    if (kcomh == 1) {
      nlhash +=1;
    } else {
      /* do not output if there is a pending header */
      if (pend_hd) {
        nlhash += 1;
      } else {
        OutString(buf);
      }
    }
    return 0;
  }

  /* Separate test for file header */
  if (filehead) {
    /*  Print immediately or exit  */
    if (kfoli == 1) {
      nldrop +=1;
    } else {
      OutString(buf);
    }
    return 0;
  }

  /* If the line is empty, only print if whitespace kept */
  if (white == 1 && hastrtxt == 0) {
    nlempt +=1 ; return 0;
  }
     
  /* For 'wrong author': same thing */
  if (auth != ' ') {
    if (lineauth != ' ' && lineauth != auth) {
      nldrop += 1; return 0;
    }
  }

  /* Now print any pending page header */
  if (pend_hd) {
    /* fprintf(stderr, "%s\n", "Pending header output (if allowed)"); */
    pend_hd = 0;
    /* Really output only if foliation is not suppressed */
    if (kfoli == 0) {
      OutString(pgh);
      nldrop -= 1;
    }
  }

  /* The usual initialisation for 'track' */
  trackinit();

  while (cb = buf[index]) {
    TrackLight(cb, index);
    cbo = cb;
    output = 1;

    /* Check suppression of foliation comments */
    if (kfoli == 1) {
      if (in_foli != 0) {
        output = 0;
        if (cb == '>') leftjust = 2; /* triggered at the end */
      }
    }

    if (leftjust > 0) {
      if (cb == ' ' || cb == '\t') {
        output = 0;
      } else {
        leftjust -= 1;
      }
    }

    /* Check suppresion of transliterator ID */
    if (authrm && in_foli > 0) {
      if (cb == ';') authch = 1;  /* stays set until close caret */
      if (authch == 1) output = 0;
    }

    /* Check all types of inline comments. */
    /* Need to decode the type by looking ahead */
    if (in_comm == -1) comchr = buf[index+1];

    if (kcomi > 0 && in_comm != 0) {
      /* separate checks for loci and page headers */
      if (newpage) {
        if (kcomi == 2) output = 0;
      } else {
        if (comchr == '!' || comchr == '~' || comchr == '@') output = 0;
      }
    }

    /* handle drawing intrusion */
    if (in_comm && comchr == '-') {
      if (gaps == 1) output = 0;
      else if (gaps == 2) {
        if (cb == '>') cbo = '.';
        else output = 0;
      }
    }

    /* handle paragraph start and end codes */
    if (in_comm && comchr == '%') {
      if (para == 1 || para == 2) output = 0;  
    }
    if (in_comm && comchr == '$') {
      if (para == 1) output = 0;  
      else if (para == 2) {
        if (cb == '>') cbo = '\n';
        else output = 0;
      }
    }

    /* handle text tags */
    if (in_comm && comchr == '@') {
      if (kfoli == 1 || kcomi == 2) output = 0;
    }

    /* Now output if required */
    if (output != 0) {
      wrapbuf[indout++] = cbo;
    }
    index += 1;
  }
  
  /* All characters processed and added to wrapbuf */
  if (indout == 0) {
    nlempt += 1;
    return 0;
  } else if (wrapbuf[0] == '\n' && white == 1) {
    nlempt += 1;
    return 0;
  } else {
    wrapbuf[indout] = 0;
  }

  /* Now write wrapbuf whole or in pieces */
  /* Another initialisation for 'track' needed first */
  trackinit();
  
  if (wrap < 2) {
    OutString(wrapbuf);
  } else {
    /* First write all the parts */
    while ((index = FindSpace(wrapbuf, wwidth)) > 0) {
      for (ii=0; ii<=index; ii++) {
        cb = wrapbuf[ii];
        TrackLight(cb, index);
        OutChar(cb);
      }
      OutChar('/');
      OutChar('\n');
      OutChar('/');
      OutChar(' ');
      wrapbuf[0] = ' ';
      shiftl(wrapbuf, 0, index);
    }
    /* Write the last bit or handle the 'no dot found' case */
    if (index == 0) {
      OutString(wrapbuf);
    }
    if (index < 0) {
      if (mute < 2) fprintf(stderr, "%s", "No space found for line wrapping\n");
      return 1;
    }
  }

  return 0;
}

/*-----------------------------------------------------------*/

int main(int argc,char *argv[])
/*int argc;
char *argv[];*/
{
  char orig[MAXLEN], buf1[MAXLEN], buf2[MAXLEN];
  int igetl = 0, iprepl = 0, iproc = 0, iout = 0;
  int selpage, selloc;
  int erropt;

  /* For reference: */
  char *what = "@(#)ivtt\t\t1.1\t2020/04/10 RZ\n";

  /* Parse command line options */
  /* Do this first, in order to get the "mute" option first */
  erropt = ParseOpts(argc, argv);
  if (mute == 0) {
    fprintf (stderr,"Intermediate Voynich Transliteration Tool (v 1.1)\n\n");
  }

  if (erropt) {
    if (mute < 2) fprintf (stderr, "%s\n", "Error parsing command line");
    return 8;
  }

  /* List summary of options and open files as needed */  
  if (DumpOpts(argc, argv)) {
    if (mute < 2) fprintf (stderr, "%s\n", "Error opening file(s)");
    return 2;
  }  
  if (mute == 0) fprintf (stderr, "\n%s\n", "Starting...");

  clearvar();

  /* The initial value of selpage (i.e. before the first 'new folio')
     depends on whether page selection options were specified */
  if (npgopt == 0) {
    selpage = 1;
  } else {
    selpage = 0;
  }
  
  /* Main loop through input file */

  while (igetl == 0) {

    /* Read one line to buffer. 
       This concatenates lines if required but nothing more */

    igetl = GetLine(orig);
    if (igetl < 0) {  /* Normal EOF */

      /* Print statistics */
      if (mute == 0) {
        if (wrap == 0) {
          fprintf (stderr, "\n%7d lines read in\n", nlread);
        } else {
          fprintf (stderr, "\n%7d lines read in\n", nlpart);
          fprintf (stderr, "%7d lines after unwrapping\n", nlread);
        }
        fprintf (stderr, "%7d lines de-selected\n", nldrop);
        fprintf (stderr, "%7d hash comment lines suppressed\n", nlhash);
        fprintf (stderr, "%7d empty lines suppressed\n", nlempt);
        fprintf (stderr, "%7d lines written to output\n", nlwrit);
        if (wrap > 1) {
          fprintf (stderr, "%7d lines after wrapping\n", nlwrap);
        }
      }
      return 3;
    }
    else if (igetl > 0) {
      if (mute < 2) fprintf (stderr, "%s\n", "Error reading line from stdin");
      return 4;
    }

    /* here a new line was read successfully */

    nlread += 1; nlpart += 1;
    /* fprintf (stderr, "%2d %s\n", nlread, orig); */
    
    /* Preprocess line */
    /* This also keeps track of foliation and comments, and warns
       about unclosed brackets */
    cwarn = ' ';
    iprepl = PrepLine(orig, buf1);
    /* fprintf (stderr, "Line: %s\n", orig);  */
    /* fprintf (stderr, "Out : %s\n", buf1);  */
    if (iprepl != 0) {
      if (mute < 2) {
        fprintf (stderr, "%s\n", "Error preprocessing line");
        fprintf (stderr, "Line: %s\n", orig);
      }
      return 5;
    }
    if (cwarn != ' ') {
      if (mute == 0) {
        fprintf (stderr, "%c %s\n", cwarn, "warning for line:");
        fprintf (stderr, "%s\n", orig);
      }
      cwarn = ' ';
    }

    /* Check page variables when new page read */
    if (newpage == 1) {
      pend_hd = 0;
      selpage = usepgloc(1);
      if (nlcopt || hastag) {
        selloc = 0;
        pend_hd = 1;
        /* fprintf(stderr, "%s\n", "Pending header saved:");
        fprintf(stderr, "%s\n", buf1);            */
        (void) strcpy(pgh, buf1);
      } else {
        selloc = 1;
      }
      /*
        fprintf (stderr,"Variables on this new folio:\n");
        (void) showvar();
        fprintf (stderr,"Select page: %d\n",selpage);
      */
    }

    /* Check page variables, text tags and locus types for all
       transliteration items */
    if (newpage == 0) {
      selpage = 1;
      selloc = usepgloc(0);
    }
  
    /* Only continue if page and locus pass the criteria */
    if ((selpage == 0) || (selloc == 0)) {
      nldrop += 1;
    } else {

      /* Process spaces */
      if (ProcSpaces(buf1, buf2) != 0) {
        if (mute < 2) {
          fprintf (stderr, "%s", "Error processing spaces\n");
          fprintf (stderr, "Line: %s\n", orig);
        }
        return 6;
      }

      /* Process uncertain readings */

      iproc = ProcRead(buf2);
      if (iproc > 0) {
        if (mute < 2) {
          fprintf (stderr, "%s\n", "Error processing uncertain readings");
          fprintf (stderr, "Line: %s\n", orig);
        }
        return 7;
      } else {

        /* Further processing and output are skipped if
           locus was not selected */
        if (iproc == 0) {
      
          /* Write line to stdout. Here the optional comment and/or
             foliation removal and/or author selection is handled, 
             as well as any re-wrapping. */

          if (PutLine(buf2) != 0) {
            if (mute < 2) fprintf (stderr, "Line: %s\n", orig);
            return 9;
          }
        } else {
          nldrop += 1;
        }
      }
    }  
  }
}
