#include "stdio.h"
#include "string.h"
#include "ctype.h"
#define WIDTXT 2048
#define MAXRUL 2048
#define WIDRUL 64
#define MAXDEF 512
#define MAXCOM 6

/*
   Bi-Directional Translation / Substitution Tool.
   by R.Zandbergen. ver 1.4, 19 September 2021.
*/

/* Global variables */
/* The following capture the information from the command line options. */

int bitdir=1;    /* Translation direction, must be 1 or 2 */
int debr=0;      /* Debugging output levels */
int debs=0;
int strict=0;    /* Be strict about matching transliteration alphabets */
int mute=0;      /* Normal output to stderr, or less */

int infarg= -1;  /* Argument of input file name */
int oufarg= -1;  /* Argument of output file name */
int rufarg= -1;  /* Argument of rules file name */


/* Other global variables for bitrans */

FILE *fin, *fout, *frul; /* File handles */
FILE *fdeb;              /* Debug file handle */

char orig[WIDTXT], text[WIDTXT], modt[WIDTXT], spcs[WIDTXT];

char chutf[3];    /* to store UTF-8 strings */

int lenorig, lentext;
int bitfr, bitto; /* 0 or 1 to reflect the from,to indices */
int ndef = 0;     /* Number of definitions in the rules file */
                  /* For this variable, 0 means zero and 1 means 1 */
int ncom = 0;     /* Number of comment definitions in the rules file */
	          /* For this variable, 0 means zero and 1 means 1 */
char csep= '#';   /* The placeholder for spaces when matching */
char camp= '&';   /* The ampersand character */
int  poly = 0;    /* Rules file has homophonic records? */
int  blkrec = 0;  /* A rules sorting block record or not? */

char rucodi[ ] = "    ";
char rucodo[ ] = "    ";
int  samecode;
int  levout = -1;   /* Optional STA level for output file */
int  hascr          /* >0 if an input file includes CR characters */;
char lcom[MAXCOM][2];
int ip[2][MAXDEF][2];  /* Pointers to the collected replacement strings */
int lip[2][MAXDEF];    /* The real lengths of the replacement strings */
int lipsor[2][MAXDEF];    /* Their lengths for the purpose of sorting */
int lstr[2];           /* The total lengths, not including the final NULLs */
char rulz[2][MAXRUL];  /* The actual strings. It has NULLS too. */
int ix[MAXDEF];        /* (Forward) sorting list */
int sblk[MAXDEF];      /* Sorting block points */

/* The following are for the homophonic option */
int istrlo[MAXDEF];    /* Pointer to first (unsorted) output string option */
int istrhi[MAXDEF];    /* Pointer to last  (unsorted) output string option */

long int locseed = 1;  /* Initial seed for local random function */
int nrulw;             /* Number of words in rules file record */
int irulw0[8];         /* Points to first chars of words in rules file record */
int irulw1[8];         /* Points to last  chars of words in rules file record */

/* Some file stats */
int nlread = 0   /* Number of lines from input file */;
int ivtfform = 0 /* value 1 if the input file has an IVTFF header, set in main */;


/*-----------------------------------------------------------*/

void shiftl(int index, int nrlost)

/* Shift left by (nrlost) bytes the part of the three
   strings text, modt and spcs,
   STARTING AT (index), which can be 0 or higher */
/* This also moves the terminating NULLs */

{
  int jj, lold, lnew;

  lold = strlen(text);
  lnew = lold - nrlost;
  for (jj = index; jj<=lnew; jj++) {
    text[jj] = text[jj+nrlost];
    modt[jj] = modt[jj+nrlost];
    spcs[jj] = spcs[jj+nrlost];
  }
  return;
}

/*-----------------------------------------------------------*/

void shiftr(int index, int nradd)

/* Shift right by (nradd) bytes the part of the three
   strings text, modt and spcs,
   STARTING AT (index), which can be 0 or higher. 
   Fill the new spot(s) with space(s).
   Assumes that length checks have been done */
/* This also moves the terminating NULL */

{
  int jj, lold, lnew;

  lold = strlen(text);
  lnew = lold + nradd;
  for (jj = lold; jj>=index; jj--) {
    text[jj+nradd] = text[jj];
    modt[jj+nradd] = modt[jj];
    spcs[jj+nradd] = spcs[jj];
  }
  for (jj = index; jj<index+nradd; jj++) {
    text[jj] = ' ';
    modt[jj] = ' ';
    spcs[jj] = ' ';
  }
  return;
}

/*-----------------------------------------------------------*/

int cha2in(char cha)

/* Convert character to one byte (hexadecimal) */
/* Requires 0-9 or A-F (upper case) */
/* Returns 0-15, or -1 in case of error */

{
  int it;

  it = ((int) cha) - 48;
  if (it < 0) return -1;
  if (it < 10) return it;
  it -= 7;
  if (it > 15) return -1;
  return it;

}

/*-----------------------------------------------------------*/

int utfenc(int iutf, char *chutf)

/* Converts an integer (0-65535) to a 1-3 character UTF-8 code */
/* Returns length (1-3), or -1 in case of failure */
/* Output string is NOT null-terminated */

{
  int i1, i2, i3;

  if (iutf < 0) return -1;

  if (iutf < 128) {
    chutf[0] = (char) iutf;
    return 1;
  }

  if (iutf < 2048) {
    i1 = iutf / 64;
    i2 = iutf - 64*i1;
    chutf[0] = (char) (192+i1);
    chutf[1] = (char) (128+i2);
    return 2;
  }

  if (iutf > 65535) return -1;

  i2 = iutf / 64;
  i3 = iutf - 64*i2;
  i1 = i2/64;
  i2 = i2 - 64*i1;
  chutf[0] = (char) (224+i1);
  chutf[1] = (char) (128+i2);
  chutf[2] = (char) (128+i3);
  return 3;

}

/*-----------------------------------------------------------*/

int localrand(int nmax)

/* Return a random number in the interval 0 - nmax */
/* Will only be called for nmax >= 1 */

{
  int jtemp, jdiv, jmod;

  jmod = 0;

  locseed *= 1103515245;
  locseed += 12345;
  locseed = (locseed / 65536) % 2048;

  jmod = locseed % (nmax+1);

  return jmod;

}

/*-----------------------------------------------------------*/

int cindex(char *cn,char *cwide, int ipos)

/* A variation of Fortran 'index' function. Search for cn inside
   cwide, but from position ipos onwards. 
   Both must be null-terminated.
   Failure causes a return value of -1  */

{
  int ioff, lcn, lcw;
  int iomax, icomp;

  if (ipos < 0) {
    return -1;
  }

  ioff = ipos;
  lcn = strlen(cn);
  lcw = strlen(cwide);
  iomax = lcw-lcn;
  /* fprintf(stderr,"iomax: %3d \n",iomax); */

  while (ioff <= iomax) {
    icomp = strncmp(cn,&cwide[ioff],lcn);
    /* fprintf(stderr,"ioff: %3d  icomp: %3d \n",ioff,icomp); */
    if (icomp == 0) {
      return ioff;
    }
    ioff +=1;
  }
  return -1;
}

/*-----------------------------------------------------------*/

int rulget(char *buf)

/* Improved rule parser that counts the number of items */

{
  int lenc;
  int jj, phase;

  /* Return <0 if line empty, >0 if some error */

  lenc = strlen(buf);

  /* fprintf(stderr, "-RG-- %s\n", buf);
     fprintf (stderr, "-RG-- length %3d \n", lenc); */

  phase = -1;
  nrulw = 0;

  for (jj=0; jj<lenc; jj++) {
    /* fprintf (stderr, "-RG-- %2d %c \n", jj, buf[jj]); */
    if (buf[jj] != ' ') {
      if (phase < 0) {
        /* It is the start of a new word */
        /* For the moment, the last word keeps overwriting itself */
        if (nrulw < 8) nrulw += 1;
        irulw0[nrulw-1] = jj;
        phase = 1;
      }
    }
    if (buf[jj] == ' ') {
      if (phase > 0) {
        /* It is the first space after a new word */
        irulw1[nrulw-1] = jj-1;
        phase = -1;
      }
    }

  }  /* end of for loop */

  /* Check if the last character was a space or not */
  if (phase > 0) {
    irulw1[nrulw-1] = lenc-1;
  }

  return nrulw;

}

/*-----------------------------------------------------------*/

int utread(char *chi, char *cho, int *jj, int jend, int *lenu)

/* Convert a 4-char hex code into a 1-3 byte UTF-8 string */
/* Returns 0 in case of success, -1 if it was just an &,
   1 in case of error */

{

  int i1, i2;
  int iutf, it;

  iutf = 0;
  if (debr) {
    fprintf (fdeb, "   Checking for Unicode %d %d\n", *jj, jend);
  }

  /* on entry, jj points to the character that has the &.
     On exit: if there was no Unicode, it still points there.
     If there was, it points to the semi-colon. */

  /* Check if there are enough characters after the & */

  if ((jend - *jj) < 5) {
    if (debr) {
      fprintf (fdeb, "   -- string after & too short\n");
    }
    return -1;
  }

  /* Check if there is a semicolon in the required position */

  if (chi[*jj+5] != ';') {
    if (debr) {
      fprintf (fdeb, "   -- no terminating semicolon\n");
    }
    return -1;
  }

  /* Now we are in business */
  if (debr) {
    fprintf (fdeb, "   -- unicode detected\n");
  }
  for (i1=1; i1<5; i1++) {
    i2 = *jj + i1;
    it = cha2in( chi[i2] );
    if (it < 0) {
      /* This is an error . The message from the caller is enough */
      return 1;
    }
    iutf = 16* iutf + it;
  }
  if (debr) {
    fprintf (fdeb, "   -- %4x\n", iutf);
  }

  *lenu = utfenc(iutf,cho);
  *jj += 5;

  return 0;

}

/*-----------------------------------------------------------*/

int utproc(char *bf, char *wk, int jj0, int jje, int *lr, int *ls)

/* Copy a rules token to string "wk", while processing Unicode
   references, and doing the counts */

/* Returns 0 in case of success, 1 in case of error */

{
  int ip, jin, jout;
  int lutf, iret;
  char cutf[3];
  char ct;

  *lr = 0;
  *ls = 0;

  jin = jj0; jout = 0;

  while (jin <= jje) {

    ct = bf[jin];

    iret = -1;
    if (ct == camp) {
      iret = utread(bf, cutf, &jin, jje, &lutf);
      if (iret > 0) {
        if (mute < 2) fprintf (stderr, "E: illegal unicode\n");        
        return 1;
      }
    }

    if (iret < 0) {
      /* There was no Unicode */
      wk[jout++] = ct;
      *lr += 1;
    } else {

      /* There was a Unicode */
      for (ip=0; ip < lutf; ip++) {
        wk[jout++] = cutf[ip];
        *lr += 1;
      }
    }
   
    if (ct == csep) {
      *ls += 1;
    } else {
      *ls += 2;
    }

    /* Go to the next character */
    jin += 1;

  }
  return 0;

}

/*-----------------------------------------------------------*/

int addio(char *buf, char *w, int ii, int iolo, int iohi)

/* Add rule elements to the concatenated collections */
/* Returns 0 if OK, >0 if error */

{
  int io;
  int lenstr;
  int jj, jjbase, jjend;
  int jdefo;
  int iproc, lenr, lens;

  ndef += 1;
  if (ndef >= MAXDEF) {
    if (mute < 2) fprintf (stderr, "E: too many rules\n");        
    return 1;
  }
  sblk[ndef-1] = 0;

  /* Add the INPUT token */

  if (debr) {
    fprintf (fdeb, "   Adding input token %d\n", ii);
  }

  jjbase = irulw0[ii];
  jjend  = irulw1[ii];

  /* Move the string to the work area, while processing Unicode
     characters and computing the lengths */
  iproc = utproc(buf, w, jjbase, jjend, &lenr, &lens);
  if (iproc != 0) {
    if (mute < 2) {
      fprintf (stderr, "E: cannot process rules input token\n");        
    }
    return 1;
  }

  lenstr = lstr[0] + lenr;
  if (lenstr+1 >= MAXRUL) {
    if (mute < 2) {
      fprintf (stderr, "E: combined input tokens too long\n");        
    }
    return 1;
  }

  lip[0][ndef-1] = lenr;
  ip[0][ndef-1][0] = lstr[0]+1;
  ip[0][ndef-1][1] = lenstr;

  for (jj=0; jj<lenr; jj++) {
    rulz[0][lstr[0]+jj+1] = w[jj];
  }
  rulz[0][lenstr+1] = (char) 0;
  lstr[0] = lenstr + 1;
  lipsor[0][ndef-1] = lens;

  /* Add the OUTPUT token(s) */

  if (debr) {
    fprintf (fdeb, "   Adding output token(s) %d - %d\n", iolo, iohi);
  }

  if (ndef == 1) {
    istrlo[0] = 0;
  } else {
    istrlo[ndef-1] = istrhi[ndef-2] + 1;
  }
  jdefo = istrlo[ndef-1] - 1;

  /* Loop over all output tokens */

  for (io=iolo; io<=iohi; io++) {

    jdefo += 1;
    istrhi[ndef-1] = jdefo;

    jjbase = irulw0[io];
    jjend  = irulw1[io];

    /* Move the string to the work area, while processing Unicode
       characters and computing the lengths */
    iproc = utproc(buf, w, jjbase, jjend, &lenr, &lens);
    if (iproc != 0) {
      if (mute < 2) {
        fprintf (stderr, "E: cannot process rules output token\n");        
      }
      return 1;
    }
    lenstr = lstr[1] + lenr;
    if (lenstr+1 >= MAXRUL) {
      if (mute < 2) {
        fprintf (stderr, "E: combined output tokens too long\n");        
      }
      return 1;
    }

    lip[1][jdefo] = lenr;
    ip[1][jdefo][0] = lstr[1]+1;
    ip[bitto][jdefo][1] = lenstr;

    for (jj=0; jj<lenr; jj++) {
      rulz[1][lstr[1]+jj+1] = w[jj];
    }
    rulz[1][lenstr+1] = (char) 0;
    lstr[1] = lenstr + 1;
    lipsor[1][jdefo] = lens;

  }   /* End for io */

  if (debr) {
    fprintf (fdeb, "   Lists extent: %d : %d - %d\n", 
             ndef, istrlo[ndef-1], istrhi[ndef-1]);
  }

  return 0;

}

/*-----------------------------------------------------------*/

int getio(int jr)

/* Find the replacement string belonging to rule "jr" */
/* This is either unique, or randomly select one of several */

{
  int jlo, jhi, jro;

  jlo = istrlo[jr];
  jhi = istrhi[jr];


  if (jlo == jhi) {
    jro = jlo;
  } else {
    jro = jlo + localrand(jhi-jlo);
    if (debs) {
      fprintf (fdeb, "   Output selection: %d - %d : %d\n", 
               jlo, jhi, jro);
    }
  }

  return jro;

}

/*-----------------------------------------------------------*/

void OutLine( )

/* Write string of characters to the output file */

{
  int jj;
  char ch;

  for (jj=1; jj<lentext-1; jj++) {
    ch = text[jj];
    if (modt[jj] == ' ') {
      if (ch == csep) ch = spcs[jj];
    }
    fputc (ch, fout);
  }
  fputc ('\n', fout);
  return;
}

/*-----------------------------------------------------------*/

int ParseOpts(int argc,char *argv[])
/* Parse command line options. There can be several and each should be
   of one of the following types:
   -1, 2, -s, -mn, -vn, -f <filename>
   where n can be a small integer

   <filename>:
     Maximum two, where first is input file name and second is
     output file name  */

{
  int iar;
  char sign, optn, val;

  /* Do not yet generate any information messages */
  /* if (mute == 0) fprintf (stderr,"Parsing Command Line Options\n"); */

  iar = 1;
  while (iar < argc) {
    sign=argv[iar][0];
    if (sign == '-') {
    
      /* Normal option, not file name */
      
      optn=argv[iar][1];

      /* process each one */
      
        switch (optn) {
           case '1':
             bitdir = 1; /* Direction 1: left to right */
             break;
           case '2':
             bitdir = 2; /* Direction 1: left to right */
             break;
           case 'm':
             val=argv[iar][2];
             if (val == '0') {
               mute=0; /* All output to stderr */
             } else if (val == '1') {
               mute=1; /* suppress info and warnings */
             } else if (val == '2') {
               mute=2; /* suppress everything */
             }
             /* The setting of this option is not reported */
             break;
           case 's':
             strict=1; /* Be stict about alphabet IDs */
             break;
           case 'v':
             val=argv[iar][2];
             if (val == '0')  {
               debr = 0;     /* No rules debugging */
               debs = 0;     /* No substitution debugging */
             } else if (val == '1') {
               debr = 1;
               debs = 0;
             } else if (val == '2') {
               debr = 0;
               debs = 1;
             } else {
               debr = 1;
               debs = 1;
             }
             break;
           case 'f':
             iar += 1;
             rufarg = iar;
             break;
      }
    } else {
      /* A file name. Check which of two */
      if (infarg < 0) {
        infarg = iar;
      } else if (oufarg < 0) {
        oufarg = iar;
      } else {
        /* The error message is generated in main */
        return 1;
      }
    }
    iar += 1;
  }
  return 0;
}

/*-----------------------------------------------------------*/

int DumpOpts(int argc,char *argv[])

/* Print information on selected options
   and open input and output files as required */

  {
  if (mute == 0) {
    fprintf (stderr,"%s\n","Summary of options:");

    /* Process each one */
    switch (bitdir) {
       case 1: fprintf (stderr,"%s\n","Translation direction 1 - left to right");
           break;
       case 2: fprintf (stderr,"%s\n","Translation direction 2 - right to left");
           break;
    }
    switch (strict) {
       case 0: fprintf (stderr,"%s\n","No alphabet name checking");
           break;
       case 1: fprintf (stderr,"%s\n","Alphabet names must match");
           break;
    }
    switch (debr+debs) {
       case 0: fprintf (stderr,"%s\n","No additional debug output");
           break;
    }
    switch (debr) {
       case 1: fprintf (stderr,"%s\n","Rules debugging output");
           break;
    }
    switch (debs) {
       case 1: fprintf (stderr,"%s\n","Substitution debugging output");
           break;
    }

  }  /* End of: if (mute == 0) */

  /* Process direction */
  bitfr = bitdir - 1;
  bitto = 1 - bitfr;

  /* List and open the Files */

  /* Input file */
  if (mute == 0) fprintf (stderr,"\nInput file: ");
  if (infarg >= 0) {
    if (mute == 0) fprintf(stderr, "%s\n",argv[infarg]);
    if ((fin = fopen(argv[infarg], "r")) == NULL) {
      if (mute < 2) fprintf(stderr, "E: input file does not exist\n");
      return 1;
    }
  } else {
    fin = stdin;
    if (mute == 0) fprintf (stderr,"<stdin>\n");
  }
  
  /* Output file */
  if (mute == 0) fprintf (stderr,"Output file: ");
  if (oufarg >= 0) {
    if (mute == 0) fprintf(stderr, "%s\n", argv[oufarg]);
    if ((fout = fopen(argv[oufarg], "w")) == NULL) {
      if (mute < 2) fprintf(stderr, "E: cannot open output file\n");
      return 1;
    }
  } else {
    fout = stdout;
    if (mute == 0) fprintf (stderr, "<stdout>\n");
  } 

  /* Rules file */
  if (mute == 0) fprintf (stderr,"Rules file: ");
  if (rufarg < 0) {
    if (mute == 0) fprintf(stderr, "%s\n", "bit_rules.txt");
    frul = fopen("bit_rules.txt", "r");
  } else {
    if (mute == 0) fprintf(stderr, "%s\n", argv[rufarg]);
    frul = fopen(argv[rufarg], "r");
  }
  if (frul == NULL) {
    if (mute < 2) fprintf(stderr, "E: rules file does not exist\n");
    return 1;
  }
      
  /* Debug output */

  if (debr || debs) fdeb = fopen("bit_debug.txt", "w");

  return 0;
}

/*-----------------------------------------------------------*/

int GetLine(char *buf, FILE *fh, int max)

/* Get a line from some input file to some buffer */
/* The line is expected to end with newline, but this is not
   saved in the buffer. Instead, it will end with a NULL */
/* Return 0 if all OK, <0 if EOF, 1 if error */
/* -1 if EOF after newline, -2 if EOF after partial line */

{
  int cr = 0, eod = 0, index = 0, iget;
  int jj;
  char cget;

  while (cr == 0 && eod == 0) {
    iget = fgetc(fh);
    /* fprintf(stderr, "Index %3d,  char %3d\n", index, iget); */

    /* Check for end of file. Only normal at first read */
    eod = (iget == EOF);
    if (eod) {
      if (index == 0) {
        return -1;       /* Correct EOF */
      } else {
        buf[index] = 0;  /* Add a null character */
        if (mute < 2) {
          fprintf (stderr, "W: EOF at record pos. %4d\n", index);        
          fprintf (stderr, "   Line read so far: %s\n", buf); 
          fprintf (stderr, "   Line will be used\n"); 
        }
        return -2;
      }
    }

    cget = (char) iget;
     
    /* Now check for CR character */
    if (cget == '\r') {
      /* Don't do any clever checks yet */
      hascr = 1;
      /* This is simply skipped - continue with next read */
    } else {
      /* Should add the character to the buffer */
      /* However, first check for buffer overflow */
      if (index >= (max-2)) {
        buf[index] = 0;
        if (mute < 2) {
          fprintf (stderr, "E: record too long.\n");        
          fprintf (stderr, "Line read so far: %s\n", buf); 
        }
        return 1;
      }

      /* Check for newline */
      if (cget == '\n') {
        buf[index] = 0;    /* Add null at end of line */
        cr = 1;            /* This causes the while loop to finish */
      } else {
        /* Add the character and allow to continue */
        buf[index++] = cget;
      }
    }
  }

  return 0;
}

/*-----------------------------------------------------------*/

void ShowRules( )

/* Temporary debug output for testing the processing */

{
  int jj;
  unsigned int ich;

  fprintf(fdeb,"\n");
  fprintf(fdeb,"Nr rules: %4d\n", ndef);
  fprintf(fdeb,"Length 1: %4d\n", lstr[0]);
  for (jj=0; jj<20; jj++) {
    ich = (int) rulz[0][jj];
    /* fprintf(fdeb,"   %2x\n",ich); */
    fprintf(fdeb,"   %2x\n",rulz[0][jj]);
  }
  fprintf(fdeb,"\n");
  fprintf(fdeb,"Length 2: %4d\n", lstr[1]);
  for (jj=0; jj<20; jj++) {
    ich = (int) rulz[1][jj];
    /* fprintf(fdeb,"   %2x\n",ich); */
    fprintf(fdeb,"   %2x\n",rulz[1][jj]);
  }
  fprintf(fdeb,"\n");
  return;
}

/*-----------------------------------------------------------*/

void ShowLines( )

/* Temporary debug output for testing the processing */

{
  fprintf(fdeb,"%s\n",text);
  fprintf(fdeb,"%s\n",modt);
  fprintf(fdeb,"%s\n",spcs);
  fprintf(fdeb,"\n");

}

/*-----------------------------------------------------------*/

int ReadRules( )

/* Read the rules file to memory.  Return 0 if all OK, 1 if error */
   
{
  int igetr=0, rulehead=0, nlin=0;
  int eorulf;
  int lcrul, jj, iritg;
  int i0;
  int iw;
  int len01, len23, lentst;
  int jj0, jj1, jj2, jj3;
  int iadd;
  char crul[WIDRUL];  /* Contents of one rules file entry */
  char work[WIDRUL];  /* Temporary work area */
  char ctest[ ] = "##BIT";

  char cdir;
  lstr[0] = -1; lstr[1] = -1;
  eorulf = 0;

  if (mute == 0) fprintf(stderr,"\n%s\n","Reading rules file");

  while (igetr == 0) {

    /* Read one rules line to buffer */

    igetr = GetLine(crul,frul,WIDRUL);
    eorulf = (igetr < 0);
    if (igetr == -1) {  /* Normal EOF */
      return 0;
    }
    if (igetr > 0) {  /* An error */
      return 2;
    }

    /* Now a line was read */
    nlin += 1;

    lcrul = strlen(crul);

    blkrec = 0;

    /* Parse the line using the new routine */
    iritg = rulget(crul);

    if (debr) {
      fprintf (fdeb, "-> %s\n", crul);
      fprintf (fdeb, "---> line nr: %3d \n", nlin);
      fprintf (fdeb, "---> length:  %3d \n", lcrul); 
      fprintf (fdeb, "---> words:    %2d \n", iritg);
    }
    
    /* Check for the header */

    if (nlin == 1) {
      rulehead = 1;
      for (jj=0; jj<=4; jj++) {
        if (crul[jj] != ctest[jj]) rulehead = 0;
      }
      if (rulehead == 0) {
        if (mute < 2) fprintf (stderr, "E: rules file has no valid header\n");        
        return 1;
      }
      if (lcrul > 5) {
        cdir = crul[5];
      } else {
        cdir = ' ';
      }
      if (cdir == ' ') {
        if (mute == 0) fprintf (stderr, "Rules file is bi-directional\n");        
      } else {
        if (mute == 0) fprintf (stderr, "Rules file enforces direction %c \n",cdir);        
        if ((cdir == '1') && (bitdir == 2)) {
          if (mute < 2) {
            fprintf (stderr, "E: user-requested direction 2 forbidden\n");        
          }
          return 1;
        }
        if ((cdir == '2') && (bitdir == 1)) {
          if (mute < 2) {
            fprintf (stderr, "E: user-requested direction 1 forbidden\n");        
          }
          return 1;
        }
      }

      /* Here record the alphabets, if the record seems long enough to include them */
      if (lcrul >= 16) {
        for (jj = 0; jj<4; jj++) {
          if (bitdir == 1) {
            rucodi[jj] = crul[7+jj];
            rucodo[jj] = crul[12+jj];
          } else {
            rucodi[jj] = crul[12+jj];
            rucodo[jj] = crul[7+jj];
          }
        }
        if (lcrul >= 19) {
          /* Future code to interpret the STA level */
        }
      }

    } else {

      /* here it was not the first (header) record */
      if (iritg == 0) {
        if (mute == 0) fprintf(stderr,"W: empty rules record ignored\n");
      }

      if (iritg == 1) {
        /* Here the record has only one word */
        /* Check for separator re-definition and comment records */
        if (debr) {
          fprintf(fdeb,"---> single-word record\n");
        }
        /* Separator re-definition first, only allowed on the second line*/
        if ((crul[0] == '#') && (crul[1] == '=')) {
          if (nlin != 2) {
            if (mute < 2) fprintf (stderr, "E: invalid line for #= record\n");        
            return 1;
          }
          csep = crul[2];
          if (mute == 0) {
            fprintf (stderr, "Separator redefined as %c \n",csep);        
          }
          if (csep == camp) {
            if (mute < 2) fprintf (stderr, "E: separator & not allowed\n");        
            return 1;
          }

        } else {
          /* Single word but not separator re-definition */
          /* Check for a comment rule */

          i0 = cindex("(comment)",crul,0);
          if (debr) fprintf(fdeb, "-----> comment index = %2d \n", i0);
          if (i0 >= 0) {

            /* Comment record. Process it */
            if (ncom >= MAXCOM) {
              if (mute < 2) fprintf (stderr, "E: too many comment records\n");        
              return 1;
            }
            /* fprintf(stderr,"Len comm %2d\n",lcrul-i0); */
            ncom += 1;
            lcom[ncom-1][0] = crul[i0-1];
            if (lcrul-i0 > 9) {
              lcom[ncom-1][1] = crul[i0+9];
            } else {
              lcom[ncom-1][1] = ' ';
              if (mute < 2) {
                fprintf (stderr, "W: comment record short - space added\n");        
              }
            }
        
          } else {

            /* Finally check for a rules sort blocker ------ */

            if (lcrul == 6) {
              blkrec = 1;
              for (jj=0; jj<=5; jj++) {
                if (crul[jj] != '-') blkrec = 0;
              }

            }

            if (blkrec == 1) {

              if (debr) {
                fprintf(fdeb, "-----> rules block record\n");
              }
              if (ndef == 0) {
                if (mute < 2) {
                  fprintf (stderr, "W: rules block record before rules ignored\n");        
                }
              } else {
                sblk[ndef-1] = 1;
              }

            } else {

              /* An unrecognised record of only one word. */
              if (mute < 2) {
                fprintf (stderr, "E: single-word record not recognised\n");        
              }
              return 1;

            }
          }
        }

      }    /* End of case: iritg == 1 */

      if (iritg > 1) {

        /* A substitution record. Process it */

        /* ShowRules( ); */

        if (debr) {
          fprintf(fdeb, "---> %2d %2d %2d %2d \n", irulw0[0],irulw1[0],
                                                   irulw0[1],irulw1[1]);
        }

        if (iritg > 2) {
          poly = bitdir;
        }

        /* Add it to the collection of replacement strings */

        if (bitfr == 0) {

          /* One input with possibly several outputs */
          iadd = addio(crul,work,0,1,iritg-1);
          if (iadd != 0) {
            if (mute < 2) {
              fprintf (stderr, "E: cannot add rule to structure\n");        
            }
            return 1;
          }

        } else {

          /* Possibly several inputs all with the same output */
          for (iw=1; iw<iritg; iw++) {
            iadd = addio(crul,work,iw,0,0);
            if (iadd != 0) {
              if (mute < 2) {
                fprintf (stderr, "E: cannot add rule to structure\n");        
              }
              return 1;
            }
          }
        }

        /* ShowRules ( ); */

      }     /* end if iritg > 1 */
    }      /* end if nlin == 1 */

    if (eorulf) {
      /* This is the case where EOF was found after a partial line */
      return 0;
    }
  }    /* end while igetr == 0 */
}

/*-----------------------------------------------------------*/

int SortRules( )

/* Sort the rules that were stored in memory. 
   Return 0 if all OK, 1 if error */
   
{
  int didswap, ambig;
  int jjend, js1;
  int ii1, ii2, ipi1, ipi2, leni1, leni2;
  int lens1, lens2;
  int icomp;
  int numcheck;

  /* This routine implements a bubble sort looking at the lenghts */
  /* This is different from the fortran implementation */
  /* From version 1.4 onwards, it uses 'effective lengths' */

  if (mute == 0) {
    fprintf(stderr, "\nAnalysing %3d substitution rules\n", ndef);
  }

  if (ndef<2) {
    if (mute == 0) {
      fprintf(stderr, "Only one rule, no analysis necessary\n");
    }
    return 0;
  }

  ambig = 0;

  /* Outer loop 1, to check for conflicting / multiple rules */

  numcheck = 0;
  if (debr) {
    fprintf(fdeb, "Checking...\n");
  }
  for (ii1=0; ii1<ndef-1; ii1++) {

    for (ii2=ii1+1; ii2<ndef; ii2++) {

      numcheck += 1;

      ipi1 = ip[0][ii1][0];
      ipi2 = ip[0][ii2][0];
      /* leni1 = ip[0][ii1][1] - ipi1 + 1;
         leni2 = ip[0][ii2][1] - ipi2 + 1; */
      leni1 = lip[0][ii1];
      leni2 = lip[0][ii2];

      if (debr) {
        if (numcheck < 5000) {
          fprintf(fdeb, " comp1: L=%d, %s\n", leni1,&rulz[0][ipi1]);
          fprintf(fdeb, " comp2: L=%d, %s\n", leni2,&rulz[0][ipi2]);
        }
      }

      if (leni2 == leni1) {
        /* Check for repeated rules */
        icomp = strncmp(&rulz[0][ipi1],&rulz[0][ipi2],leni1);
        if (icomp == 0) {
          if (mute < 2) {
            fprintf(stderr, "E: multiple rules for %s\n", &rulz[0][ipi1]);
          }
          ambig = 1;
        }
      }
    }   /* End of first (inner) for loop */

  }   /* End outer loop 1 */

  /* Initialise the sorting */

  for (js1=0; js1<ndef; js1++) {
    ix[js1]=js1;
  }

  /* Outer loop 2, bubble sort by length */

  if (debr) {
    fprintf(fdeb, "\nSorting...\n");
  }
  didswap = 1;
  jjend = ndef-1;

  while (didswap && (jjend >=0)) {

    didswap = 0;

    for (js1=0; js1<jjend; js1++) {

      ii1 = ix[js1];
      ii2 = ix[js1 + 1];

      ipi1 = ip[0][ii1][0];
      ipi2 = ip[0][ii2][0];
      /* leni1 = ip[0][ii1][1] - ipi1 + 1;
         leni2 = ip[0][ii2][1] - ipi2 + 1; */
      leni1 = lip[0][ii1];
      leni2 = lip[0][ii2];
      lens1 = lipsor[0][ii1];
      lens2 = lipsor[0][ii2];

      if (debr) {
        fprintf(fdeb, " comp1: L=%d, Ls=%d %s\n", leni1,lens1,&rulz[0][ipi1]);
        fprintf(fdeb, " comp2: L=%d, Ls=%d %s\n", leni2,lens2,&rulz[0][ipi2]);
      }
      if (lens2 > lens1) {
        /* Do the swap if it is not blocked by a block record */
        if (sblk[js1] == 0) {
          ix[js1+1] = ii1;
          ix[js1] = ii2;
          if (debr) fprintf(fdeb, " swapped\n");
          didswap = 1;
        } else {
          if (debr) fprintf(fdeb, " swap blocked\n");
        }
      }

    }      /* End of second (inner) for loop */

    jjend -=1;

  }       /* end of second outer loop (while jjend >= 0) */

  return ambig;
}

/*-----------------------------------------------------------*/

int PrepLine( )

/* Prepare the line just read from the input file */
/* Return 0 if all OK, >0 if there is some error */
/* Return -1 if the entire line is a comment */

{
  int jj, jc;
  int iret;
  char ch, clcom;
  int issep;


  /* Add a space at the start and at the end */
  text[lentext] = ' ';
  lentext += 1;
  text[lentext] = '\0';
  shiftr(0,1);
  lentext += 1;

  /* Check for full line comment */
  iret = 0;
  for (jc=0; jc<ncom; jc++) {
    if (lcom[jc][1] == ' ') {
      if (text[1] == lcom[jc][0]) iret = -1;
    }
  }
  if (iret < 0) return iret;

  /* Process the other comments, initialising "modt" */

  clcom = ' ';
  jj = 0;

  while (jj < lentext) {
    modt[jj] = ' ';
    if (clcom == ' ') {
      /* Looking for start of a comment */
      for (jc=0; jc<ncom; jc++) {
        if (lcom[jc][1] != ' ') {
          if (text[jj] == lcom[jc][0]) {
            clcom = lcom[jc][1];
            modt[jj] = '-';
          }
        }
      }
    } else {
      /* Looking for end of a comment */
      modt[jj] = '-';
      if (text[jj] == clcom) clcom = ' ';
    }

    jj += 1;
  }

  /* Initialise the "spcs" help string */
  for (jj=0; jj<lentext; jj++) {
    spcs[jj] = ' ';
    if (modt[jj] == ' ') {
      ch = text[jj];
      issep = 0;
      if (ch == ' ' || ch == '.' || ch == ',') issep = 1;
      if (issep) {
        spcs[jj] = text[jj];
        text[jj] = csep;
      } else {
        spcs[jj] = '+';
      }
    }
    /* Make sure that original instances of the separator
       symbol (1) will be preserved (2) will not be interpreted */
    if (ch == csep) {
      spcs[jj] = csep;
      modt[jj] = '-';
    }
  }
  return 0;
}

/*-----------------------------------------------------------*/

int ProcLine( )

/* Perform all substitutions on the line */

{
  int jd, jr, jro, jpi, jpo, jj, jc;
  int jdout;
  int iret;
  int loc, newloc;
  int leni, leno, dlen;
  char ch, clcom, sepkeep;
  int issep, isfree;

  for (jd=0; jd<ndef; jd++) {
    jr = ix[jd];
    /* Input pointers are fixed for this rule */
    /* Output pointers may vary in case of ambiguous substitutions */
    jpi = ip[0][jr][0];
    leni = lip[0][jr];
    if (debs) {
      fprintf(fdeb, " testing %s\n",&rulz[0][jpi]);
      fprintf(fdeb, " input  length: %3d\n",leni);
    }

    /* Repeatedly search for this token */
    loc = 0;
    newloc = 0;

    while (loc >= 0) {

      loc = cindex(&rulz[0][jpi],text,newloc);
      if (loc <0) {
        if (debs) fprintf(fdeb, "   not found\n");
      } else {
        if (debs) fprintf(fdeb, "   found at position %3d\n",loc);
        isfree = 1;

        /* Select the output string */
        jro = getio(jr);
        jpo = ip[1][jro][0];
        leno = lip[1][jro];
        if (debs) {
          fprintf(fdeb, " output length: %3d\n",leno);
        }

        /* set default new separator */
        if (ivtfform) {
          sepkeep = '.';
        } else {
          sepkeep = ' ';
        }
        for (jj=0; jj<leni; jj++) {
          if (modt[loc+jj] != ' ') isfree = 0;
          if (text[loc+jj] == csep) sepkeep = spcs[loc+jj];
        }
        if (isfree) {
          if (debs) fprintf(fdeb, "    to be replaced\n");

          newloc = loc + leno;

          /* Shift the text in case there is a length change */
          if (leno > leni) {
            dlen = leno - leni;
            if (debs) {
              fprintf(fdeb, "    inserting space for %2d chars\n", dlen);
            }
            shiftr(loc,dlen);
            lentext += dlen;
          } 
          if (leno < leni) {
            dlen = leni - leno;
            if (debs) fprintf(fdeb, "    removing %2d chars\n", dlen);
            shiftl(loc,dlen);
            lentext -= dlen;
          } 
          for (jj=0; jj<leno; jj++) {
            text[loc+jj] = rulz[1][jpo+jj];
            if (text[loc+jj] == csep) {
              spcs[loc+jj] = sepkeep;
            } else {
              spcs[loc+jj] = ' ';
              modt[loc+jj] = '-';
            }
          }
          if (debs) ShowLines( );
        } else {
          if (debs) fprintf(fdeb, "    cannot replace\n");
          newloc = loc + leni;
        }
      }

    }    /* End of while loop for each token */

  }     /* End for loop over all different tokens */
  return 0;
}

/*-----------------------------------------------------------*/

int main(int argc,char *argv[])

{
  int igetl = 0;
  int eodata;
  int erropt, jj;
  int icomp, iretc;
  int lprint;
  char ctest[ ] = "#=IVTFF ";

  /* For reference: */
  char *what = "@(#)bitrans\t\t1.4\t2021/09/19 RZ\n";

  /* Parse command line options */
  /* Do this first, in order to get the "mute" option before gnerating output */
  erropt = ParseOpts(argc, argv);
  if (mute == 0) {
    fprintf (stderr,"Bi-directional translation / substitution tool (v 1.4)\n\n");
  }

  if (erropt) {
    if (mute < 2) {
      fprintf (stderr, "%s\n", "E: only two file names allowed");
      fprintf (stderr, "%s\n", "   error parsing command line");
    }
    return 8;
  }

  /* List summary of options and open files as needed */  
  if (DumpOpts(argc, argv)) {
    if (mute < 2) fprintf (stderr, "%s\n", "  error opening file(s)");
    return 2;
  }  

  /* Read and analyse/sort the Rules file */
  hascr = 0;
  if (ReadRules( )) {
    if (mute < 2) fprintf (stderr, "%s\n", "  error reading rules file");
    return 2;
  }  
  if (hascr) {
    if (mute < 2) fprintf (stderr, "%s\n", "W: rules file has CR characters");
  }

  if (mute == 0) {
    fprintf (stderr,"Alphabet codes in rules file: %4s %4s\n", rucodi,rucodo);
    fprintf (stderr, "%3d comments defined\n", ncom);
    fprintf (stderr, "%3d substitution rules\n", ndef);
    if (poly == 1) {
      fprintf (stderr,"Rules file encodes ambiguous definitions\n");
    }
    if (poly == 2) {
      fprintf (stderr,"Rules file decodes ambiguous definitions\n");
    }
  }

  /* Temporary test output */
  if (debr) ShowRules( );

  if (SortRules( )) {
    if (mute < 2) fprintf (stderr, "%s\n", "  error found in rules file");
    return 2;
  }  

  if (mute == 0) fprintf (stderr, "\n%s\n", "Starting...");

  /* Main loop through input file */

  hascr = 0;
  while (igetl == 0) {

    /* Read one line to buffer. */

    igetl = GetLine(orig,fin,WIDTXT);
    if (igetl == -2) {
      if (mute < 2) fprintf (stderr, "E: incomplete record before EOF\n");
      return 2;
    }
    if (igetl == -1) {  /* Normal EOF */

      if (hascr) {
        if (mute < 2) fprintf (stderr, "%s\n", "W: input file has CR characters");
      }
      /* Print statistics */
      if (mute == 0) {
        fprintf (stderr, "\n%7d lines processed\n", nlread);
      }
      return 0;
    }
    else if (igetl > 0) {
      if (mute < 2) fprintf (stderr, "%s\n", "  error reading line from input");
      return 4;
    }

    /* Here a new line was read successfully */
    (void) strcpy(text,orig);
    lenorig = strlen(orig);
    lentext = lenorig;

    /* Check for an IVTFF header */

    if (nlread == 0) {
      ivtfform = 1;
      for (jj=0; jj<7; jj++) {
        if (text[jj] != ctest[jj]) ivtfform = 0;
      }
      if (ivtfform == 1) {
        if (mute == 0) fprintf (stderr, "Input file is IVTFF format\n");        
        icomp = strncmp(&text[8],rucodi,4);
        if (icomp == 0) {
          if (mute == 0) {
            fprintf (stderr, "Alphabet matches rules file\n");
            fprintf (stderr, "... will be replaced by %4s\n", rucodo);
            for (jj=0; jj<4; jj++) {
              text[8+jj] = rucodo[jj];
            }
          }
      
        } else {          /* here icomp != 0 */
          if (strict) {
            lprint = (mute <2);
          } else {
            lprint = (mute == 0);
          }
          if (lprint) {
            fprintf (stderr, "W: alphabet in IVTFF file: %c%c%c%c\n", 
                              text[8],text[9],text[10],text[11]);
            fprintf (stderr, "   does not match rules file: %4s\n",
                              rucodi);
            if (strict) {
              fprintf (stderr, "E: this is an error\n");
              return 2;
            }
          }
        }
      }    /* end if ivtfform==1 */
    }     /* end if nlread == 0 */

    nlread += 1;
    /* Make sure that there is no substitution debugging after 10 lines */
    if (nlread > 10) {
      if (debs) {
        fprintf (fdeb,"Substitution debugging stopped after 10 lines\n");
        debs = 0;
      }
    }

    /* fprintf (stderr, "%2d %s\n", nlread, text); */
    
    /* Here follow all the processing steps */

    iretc = PrepLine( );

    if (iretc > 0) {
      if (mute<2) {
        fprintf(stderr, "E: error pre-processing line\n");
      }
      return 2;
    }

    /* Invoke the substitution if PrepLine did not identify a comment line */
    if (iretc == 0) {
      if (debs) ShowLines( );
      iretc = ProcLine( );

      if (iretc) {
        if (mute<2) {
          fprintf(stderr, "E: error performing substitution\n");
        }
        return 2;
      }
    }

    OutLine( );

  }

}
