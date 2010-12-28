// osmfilter 2010-12-09 18:30
#define VERSION "0.3"
// (c) Markus Weber, Nuernberg
//
const char* helptext=
"\nosmfiter " VERSION "\n"
"\n"
"This program operates as filter for OSM XML data.\n"
"Only sections containing certain tags will be copied from standard\n"
"input to standard output. Use the calling line parameter -k to\n"
"determine which sections you want to have in standard output.\n"
"For example:\n"
"\n"
"  -k\"key1=val1 key2=val2 key3=val3\"\n"
"  -k\"amenity=restaurant =bar =pub =cafe =fast_food =food_court =nightclub\"\n"
"  -k\"barrier=\"\n"
"  -K/ -k\"description=something with blanks/name=New York\"\n"
"\n"
"Limitations: the maximum number of key/value pairs is 100, the maximum\n"
"length of keys and/or values will be 50. There is no warning if you\n"
"exceed these limits. Please use the option -t to print the list of\n"
"accepted search strings to standard output so you can see which\n"
"parameters are accepted by the program.\n"
"\n"
"\n"
"Considering Dependencies\n"
"------------------------\n"
"\n"
"To get dependent elements, e.g. nodes of a selected way or ways of\n"
"a selected relation, you need to feed the input OSM XML file more\n"
"than once. You need to do this at least 3 times to get the nodes of\n"
"a way which is referred to by a relation.\n"
"If you want to ensure that relations which are referred by other\n"
"relations are also processed correctly, you must input the file\n"
"a 4th time. If there are more than one inter-relational hierarchies\n"
"to be considered, you will need to do this a 5th or 6th time.\n"
"\n"
"If you feed the input file into an osmfilter more than once, you must\n"
"tell the program the exact beginning and ending of the pre-processing\n"
"sequence. For example:\n"
"\n"
"  cat lim a.osm a.osm lim a.osm | ./osmfilter -k\"lit=yes\" >new.osm\n"
"\n"
"where 'lim' is a file containing this sequence as a delimiter:\n"
"  <osmfilter_pre/>\n"
"If you have a compressed input file, you can use bzcat instead of.\n"
"cat. If this is the case, be sure to have compressed the 'lim' file\n"
"as well.\n"
"\n"
"To speed-up the filter process, the program uses some main memory\n"
"for a hash table. By default, it uses 320 MiB for storing a flag\n"
"for every possible node, 60 for the way flags, and 20 relation\n"
"flags.\n"
"Every byte holds the flag for 8 ID numbers, i.e., in 320 MiB the\n"
"program can store 2684 million flags. As there are less than 1000\n"
"million IDs for nodes at present (Oct 2010), 120 MiB would suffice.\n"
"So, for example, you can decrease the hash sizes to e.g. 130, 12 and\n"
"2 MiB using this option:\n"
"\n"
"  -h130-12-2\n"
"\n"
"But keep in mind that the OSM database is continuously expanding. For\n"
"this reason the program-own default value is higher than shown in the\n"
"example, and it may be appropriate to increase it in the future.\n"
"If you do not want to bother with the details, you can enter the\n"
"amount of memory as a sum, and the program will divide it by itself.\n"
"For example:\n"
"\n"
"  -h1000\n"
"\n"
"These 1000 MiB will be split in three parts: 800 for nodes, 150 for\n"
"ways, and 50 for relations.\n"
"\n"
"Because we are taking hashes, it is not necessary to provide all the\n"
"suggested memory; the program will operate with less hash memory too.\n"
"But, in this case, the filter will be less effective, i.e., some\n"
"nodes and some ways will be left in the output file although they\n"
"should have been excluded.\n"
"The maximum value the program accepts for the hash size is 4000 MiB;\n"
"If you exceed the maximum amount of memory available on your system,\n"
"the program will try to reduce this amount and display a warning\n"
"message.\n"
"\n"
"\n"
"Optimizing the Performance\n"
"--------------------------\n"
"\n"
"As there are no nodes which refer to other objects, preprocessing\n"
"does not need the node section of the OSM XML file. Nearly the same\n"
"applies to ways, so the ways are needed only once in preprocessing -\n"
"in the last run.\n"
"If you want to enhance performance, you should take pre-filtering the\n"
"OSM XML file into consideration. Pre-filtering can be done using the\n"
"drop option. For example:\n"
"\n"
"  cat a.osm | ./osmfilter --drop-changesets --drop-nodes >wr.osm\n"
"  cat wr.osm | ./osmfilter --drop-ways >r.osm\n"
"  cat lim r.osm wr.osm lim a.osm | ./osmfilter -k\"lit=yes\" >new.osm\n"
"\n"
"If you are using pre-filtering, there will be no other filtering,\n"
"i.e., the parameter -k will be ignored.\n"
"\n"
"There is NO WARRANTY, to the extent permitted by law.\n"
"Please send any bug reports to markus.weber@gmx.com\n\n";

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>

typedef enum {false= 0,true= 1} bool;

static int strzcmp(const char* s1,const char* s2) {
  // similar to strcmp(), this procedure compares two character strings;
  // here, the number of characters which are to be compared is limited
  // to the length of the second string;
  // i.e., this procedure can be used to identify a short string s2
  // within a long string s1;
  // s1[]: first string;
  // s2[]: string to compare with the first string;
  // return:
  // 0: both strings are identical; the first string may be longer than
  //    the second;
  // -1: the first string is alphabetical smaller than the second;
  // 1: the first string is alphabetical greater than the second;
  while(*s1==*s2 && *s1!=0) { s1++; s2++; }
  if(*s2==0)
    return 0;
  return *(unsigned char*)s1 < *(unsigned char*)s2? -1: 1;
  }  // end   strzcmp()

static int strzlcmp(const char* s1,const char* s2) {
  // similar to strzcmp(), this procedure compares two character strings;
  // and accepts the first string to be longer than the second;
  // other than strzcmp(), this procedure returns the length of s2[] in
  // case both string contents are identical, and returns 0 otherwise;
  // s1[]: first string;
  // s2[]: string to compare with the first string;
  // return:
  // >0: both strings are identical, the length of the second string is
  //     returned; the first string may be longer than the second;
  // 0: the string contents are not identical;
  const char* s2a;

  s2a= s2;
  while(*s1==*s2 && *s1!=0) { s1++; s2++; }
  if(*s2==0)
    return s2-s2a;
  return 0;
  }  // end   strzlcmp()

int readbyte(char* c) {
  // read a single byte from stdin, but use a buffer;
  // return: 1: byte has been read; -1: no (more) bytes to read;
  // *c: the byte read;
  static char readbuf[600000L];
  static char* readbufe= readbuf;
  static char* readbufp= readbuf;
//  static long bytenumber= 0;  // for statistics
  int r;

  if(readbufp>=readbufe) {  // the read buffer is empty
    r= read(0,readbuf,sizeof(readbuf));
      // fill the read buffer with bytes from stdin
    if(r<=0) {  // no more bytes to read
      *c= 0;
return -1;
      }
    readbufe= readbuf+r;
    readbufp= readbuf;
    }
  //if((++bytenumber)%100000000L==0)
    //fprintf(stderr,"MByte %li\n",bytenumber/1000000L);
  *c= *readbufp++;
  return 1;
  }  // end   readbyte();

void writestdout(const char* s,long len) {
  // write some bytes to stdout, use a buffer;
  // s[]: bytes to write;
  // len: number of bytes to write; -1: flush the buffer;
  static char writebuf[600000L];
  static char* writebufe= writebuf+sizeof(writebuf);  //-3;
  static char* writebufp= writebuf;

  if(len<0) {  // the write buffer shall be flushed
    if(writebufp>writebuf)  // at least one byte in buffer
      write(1,writebuf,writebufp-writebuf);
    writebufp= writebuf;
return;
    }
  while(--len>=0) {
    if(writebufp>=writebufe) {  // the write buffer is full
      if(writebufp>writebuf)  // at least one byte in buffer
        write(1,writebuf,writebufp-writebuf);
      writebufp= writebuf;
      }
    *writebufp++= *s++;
    }
  }  // end   writebyte();



//------------------------------------------------------------
// Module hash_   OSM hash module
//------------------------------------------------------------

// this module provides three hash tables with default sizes
// of 320, 60 and 20 MB;
// the procedures hash_set() and hash_get() allow bitwise access
// to these tables;
// as usual, all identifiers of a module have the same prefix,
// in this case 'hash'; an underline will follow in case of a
// global accessible object, two underlines in case of objects
// which are not meant to be accessed from outside this module;
// the sections of private and public definitions are separated
// by a horizontal line: ----

static bool hash__initialized= false;
#define hash__M 3
static unsigned char* hash__mem[hash__M]= {NULL,NULL,NULL};
  // start of the hash fields for each object type (node, way, relation);
static unsigned long hash__max[hash__M]= {0,0,0};
  // size of the hash fields for each object type (node, way, relation);

static void hash__end() {
  // clean-up for hash module;
  // will be called at program's end;
  int o;  // object type

  for(o= 0;o<hash__M;o++) {
    hash__max[o]= 0;
    if(hash__mem[o]!=NULL) {
      free(hash__mem[o]); hash__mem[o]= NULL; }
    }
  hash__initialized= false;
  }  // end   hash__end()

//------------------------------------------------------------

static int hash_ini(int n,int w,int r) {
  // initializes the hash module;
  // n: amount of memory which is to be allocated for nodes;
  // w: amount of memory which is to be allocated for ways;
  // r: amount of memory which is to be allocated for relations;
  // range for all input parameters: 1..4000, unit: MiB;
  // the second and any further call of this procedure will be ignored;
  // return: 0: initialization has been successful (enough memory);
  //         1: memory request had to been reduced to fit the system's
  //            resources (warning);
  //         2: memory request was unsuccessful (error);
  // general note concerning OSM database:
  // number of objects at Oct 2010: 950M nodes, 82M ways, 1.3M relations;
  int o;  // object type
  bool warning,error;

  warning= error= false;
  if(hash__initialized)  // already initialized
    return 0;  // ignore the call of this procedure
  // check parameters and store the values
  #define D(x,o) if(x<1) x= 1; else if(x>4000) x= 4000; \
    hash__max[o]= x*(1024*1024);
  D(n,0) D(w,1) D(r,2)
  #undef D
  // allocate memory for each hash table
  for(o= 0;o<hash__M;o++) {  // for each hash table
    do {
      hash__mem[o]= (unsigned char*)malloc(hash__max[o]);
      if(hash__mem[o]!=NULL) {  // allocation successful
        memset(hash__mem[o],0,hash__max[o]);  // clear all flags
    break;
        }
      // here: allocation unsuccessful
      // reduce amount by 50%
      hash__max[o]/=2;
      warning= true;
        // memorize that the user should be warned about this reduction
      // try to allocate the reduced amount of memory
      } while(hash__max[o]>=1024);
    if(hash__mem[o]==NULL)  // allocation unsuccessful at all
      error= true;  // memorize that the program should be aborted
    }  // end   for each hash table
  atexit(hash__end);  // chain-in the clean-up procedure
  if(!error) hash__initialized= true;
  return error? 2: warning? 1: 0;
  }  // end   hash_ini()

static void hash_set(int o,const char* id) {
  // set a flag for a specific object type and ID;
  // o: object type; 0: node; 1: way; 2: relation;
  //    caution: due to performance reasons the boundaries are not checked;
  // id: id of the object; the id is given as a string of decimal digits;
  //     a specific string terminator is not necessary, it is assumed
  //     that the id number ends with the first non-digit character;
  unsigned char* mem;  // address of byte in hash table
  unsigned long long idi;  // bit number (0..7)
  unsigned int ido;  // bit offset to idi;

  if(!hash__initialized) return;  // error prevention
  idi= 0;
  while(isdigit(*id)) { idi= idi*10+(*id-'0'); id++; }
    // read the identifier
  ido= idi&0x7;  // extract bit number (0..7)
  idi>>=3;  // calculate byte offset
  idi%= hash__max[o];  // consider length of hash table
  mem= hash__mem[o];  // get start address of hash table
  mem+= idi;  // calculate address of the byte
  *mem|= (1<<ido);  // set bit
  }  // end   hash_set()

static bool hash_get(int o,const char* id) {
  // get the status of a flag for a specific object type and ID;
  // o: object type; 0: node; 1: way; 2: relation;
  //    caution: due to performance reasons the boundaries are not checked;
  // id: id of the object; the id is given as a string of decimal digits;
  //     a specific string terminator is not necessary, it is assumed
  //     that the id number ends with the first non-digit character;
  unsigned char* mem;
  unsigned long long idi;
  unsigned int ido;  // bit offset to idi;
  bool flag;

  if(!hash__initialized) return 0;  // error prevention
  idi= 0;
  while(isdigit(*id)) { idi= idi*10+*id-'0'; id++; }
    // read the identifier

  ido= idi&0x7;  // extract bit number (0..7)
  idi>>=3;  // calculate byte offset
  idi%= hash__max[o];  // consider length of hash table
  mem= hash__mem[o];  // get start address of hash table
  mem+= idi;  // calculate address of the byte
  flag= (*mem&(1<<ido))!=0;  // get status of the addressed bit
//if(flag) { static i= 0; if(++i>30) flag= false; } ///
  return flag;
  }  // end   hash_get();

//------------------------------------------------------------
// end   Module hash_   OSM hash module
//------------------------------------------------------------



//------------------------------------------------------------
// Module parse_   OSM parse module
//------------------------------------------------------------

// this module contains two procedures: one to parse a section
// and setting the appropriate flags in the hash tables, and
// one to parse a key identifier to find out if the related
// flag in the hash table is set or not;
// there are no private data on module level inside this module;

void parse_hashset(const char* s) {
  // parse a given section and set the relating hash flags, if found
  // certain key identifiers;
  // s[]: characters to parse (null-terminated);
  // uses: hash_set();
  bool insidequote;  // we are between quotation marks;
  int l;  // length of matched key identifier
  int o;  // object type; 0: node; 1: way; 2: relation;

  insidequote= false;
  while(*s!=0) {  // still characters to parse
    if(*s=='\"')  // we encountered a quotation mark
      insidequote= !insidequote;
    else if(!insidequote) {  // we're not in a quoted area
      if(*s=='<') {  // a key identifier starts here
        o= 0;  // (just to get sure)
        if((l= strzlcmp(s,"<nd ref=\""))>0) o= 0;
        else if((l= strzlcmp(s,"<member type=\"node\" ref=\""))>0) o= 0;
        else if((l= strzlcmp(s,"<member type=\"way\" ref=\""))>0) o= 1;
        else if((l= strzlcmp(s,"<member type=\"relation\" ref=\""))>0) o= 2;
        if(l>0) {  // we found one of the searched key identifiers
          hash_set(o,s+l);  // set flag in hash table
          s+= l-1;  // jump over the search string
          insidequote= true;  // because a quotation mark will follow
          }
        }  // end   a key identifier starts here
      }  // end   we're not in a quoted area
    s++;  // take the next character
    }  // end   still characters to parse
  }  // end   parse_hashset()
#if 0  // OSM XML examples
  <node id="703066091" version="2" lat="49.777873" lon="9.621692"/>
  <way id="56012785" version="2" changeset="4484048">
    <nd ref="361195677"/>
  </way>
  <relation id="7044" version="4" changeset="4446778">
    <member type="node" ref="251589955" role="via"/>
    <member type="way" ref="23250694" role="from"/>
    <member type="relation" ref="14184" role=""/>
  </relation>
#endif

bool parse_hashget(const char* s) {
  // parse a given key identifier and check the hash table if the
  // relating flag has been set;
  // s[]: characters to parse (need not to be null-terminated);
  // return: related hash flag is set;
  // uses: hash_get();
  int l;  // length of matched identifier
  int o;  // object type; 0: node; 1: way; 2: relation;
  bool flag;

  o= 0;  // (just to get sure)
  if((l= strzlcmp(s,"<node id=\""))>0) o= 0;
  else if((l= strzlcmp(s,"<way id=\""))>0) o= 1;
  else if((l= strzlcmp(s,"<relation id=\""))>0) o= 2;
  flag= l>0 && hash_get(o,s+l);
return flag;
  }  // end   parse_hashget()
#if 0  // OSM XML examples
  <node id="703082561" version="1" lat="49.8479406" lon="9.9740697"/>
  <way id="56012785" version="2" changeset="4484048"> </way>
  <relation id="7044" version="4" changeset="4446778"> </relation>
#endif

//------------------------------------------------------------
// end   Module parse_   OSM parse module
//------------------------------------------------------------



int main(int argc,const char *argv[]) {
  // main program;
  // for the meaning of the calling line parameters please look at the
  // contents of helptext[];
  int insidepreproc;  // 0: no preprocessing; 1: preprocessing files at present;
    // 2: there was preprocessing;
  bool insidequote;  // we are between quotation marks;
  int nesting;  // nesting level of OSM XML objects;
    // 0: we are not inside a section of one of the defined keys which
    //    have been defined in keys[];
    // 1: we are in a section of the key with index keyi;
    // 2..: same as 1, but nested;
  static const char keys[][20]=
    {"osmfilter_pre","node","way","relation","changeset",""};
    // keys which initiate a section which may be dropped;
    // first index is reserved for the preprocessor delimiter;
    // the list is terminated by a "";
  int keyi;  // index of the key we are presently dealing with
  const char* keyp;  // key of the presently processed section
  bool preserve;  // the whole section should be preserved
  bool drop;  // the whole section is to drop; overrules 'preserve';
  bool justdropped;  // a section has just been dropped; afterwards,
    // all blanks, tabs, CRs and NLs shall be ignored;
  char c;  // character we have read
  static char buf[250000+2];
    // buffer for sections which may be dropped;
    // 50000 could be enough for huge sections (e.g. landuse=forest"),
    // but take 250000, it's saver;
  static const char* bufe= buf+sizeof(buf)-2;  
    // address of the end of the buffer
  char* bufp;  // pointer in buf[]
  static char keybuf[30000+2];  // buffer for keywords;
    // keywords are words which follow a '<' mark;
  static const char* keybufee= keybuf+sizeof(keybuf)-1-2;
    // address of the end of the keyword buffer
  char* keybufp;  // pointer in keybuf[]
  char* keybuft;  
    // pointer in keybuf[] which determines the end of the identifier
  int r;  // return code for different purposes

  // for calling line parameters
  #define pairMAX 100  // maximum number of key-val-pairs
  #define pairkMAX (50+20)  // maximum length of key or val;
    // +20, because we're creating search strings including the
    // key resp. val;
  static struct {
    char k[pairkMAX];  // key to compare;
      // "": same key as previous key in list;
    int klen;  // length of .k[];
      // 0: same length as previous key in list;
    char v[pairkMAX];  // value to the key in .k[];
      // "": any value will be accepted;
    } pair[pairMAX];
  static int pairn= 0;  // number of pairs in list 'pair[]';
  int h_n,h_w,h_r;  // user-suggested hash size in MiB, for
    // hash tables of nodes, ways, and relations;
  static bool keydrop[]= {true,false,false,false,false};
    // section must be dropped, no matter of its content;
    // element index refers to keys[];
  bool dropsect;
    // at least one section is to be dropped due to user request

  /* read command line parameters */ {
    char pairlim;  // delimiter between pairs in command line
    const char* pk,*pv,*pe;  // pointers in parameter for key/val pairs;
      // pk: key; pv: val; pe: end of val;
    bool testmode;  // user does not want processing; just print
      // the given parameter;
    int len;  // string length; different purposes;

    testmode= false;
    pairlim= ' ';
    h_n= h_w= h_r= 0;
    dropsect= false;
    if(argc<=1) {  // no command line parameters given
      write(2,helptext,strlen(helptext));  // print help text
return 0;  // end the program, because without having parameters
        // we do not know what to do;
      }
    while(--argc>0) {  // for every parameter in command line
      argv++;  // switch to next parameter; as the first one is just
        // the program name, we must do this previous reading the
        // first 'real' parameter;
      if(strzcmp(argv[0],"--drop-node")==0) {
          // remark: here, we use strzcmp() to accept "--drop-node"
          // as well as "--drop-nodes" (plural);
        keydrop[1]= true; dropsect= true;
        continue;  // take next parameter
        }
      if(strzcmp(argv[0],"--drop-way")==0) {
        keydrop[2]= true; dropsect= true;
        continue;  // take next parameter
        }
      if(strzcmp(argv[0],"--drop-relation")==0) {
        keydrop[3]= true; dropsect= true;
        continue;  // take next parameter
        }
      if(strzcmp(argv[0],"--drop-changeset")==0) {
        keydrop[4]= true; dropsect= true;
        continue;  // take next parameter
        }
      if(strcmp(argv[0],"--drop-none")==0 ||
            strcmp(argv[0],"--drop-nothing")==0) {
        dropsect= true;
        continue;  // take next parameter
        }
      if(argv[0][0]=='-' && argv[0][1]=='h' && isdigit(argv[0][2])) {
          // "-h...": user wants a specific hash size;
          // note that we accept "-h" only if it is continued by a
          // digit, so that a plain "-h" would not be recognized
          // and therefore print the help text;
        const char* p;

        p= argv[0]+2;  // jump over "-h"
        h_n= h_w= h_r= 0;
        // read the up to three values for hash tables' size;
        // format examples: "-h200-20-10", "-h1200"
        while(isdigit(*p)) { h_n= h_n*10+*p-'0'; p++; }
        if(*p!=0) { p++; while(isdigit(*p)) { h_w= h_w*10+*p-'0'; p++; } }
        if(*p!=0) { p++; while(isdigit(*p)) { h_r= h_r*10+*p-'0'; p++; } }
        continue;  // take next parameter
        }
      if(argv[0][0]=='-' && argv[0][1]=='t' && argv[0][2]==0) {
          // "-t": user wants test mode;
        testmode= true;
        continue;  // take next parameter
        }
      if(argv[0][0]=='-' && argv[0][1]=='K' && argv[0][2]!=0 && argv[0][3]==0) {
          // user wants a special pair delimiter
        pairlim= argv[0][2];
        continue;  // take next parameter
        }
      if(argv[0][0]!='-' || argv[0][1]!='k' || argv[0][2]==0) {
        write(2,helptext,strlen(helptext));  // print help text
return 0;  // end the program, because there must be something
          // wrong with the parameters;
        }
      // here: key/val pairs;
      // will be ignored later, in case one of the
      // "-drop..." parameters has been given;
      pk= argv[0]+2;  // jump over "-k"
      while(pk!=NULL && pk[0]!=0 && pairn<pairMAX) {
          // for every key/val pair
        while(*pk==pairlim) pk++;  // jump over (additional) spaces
        pe= strchr(pk,pairlim);  // get end of this pair
        if(pe==NULL) pe= strchr(pk,0);
        pv= strchr(pk,'=');  // get end of this key
        if(pv==NULL || pv>=pe-1) pv= pe;
        len= pv-pk;  // length of this key
        if(len>(pairkMAX-20)) len= pairkMAX-20;  // delimit key length
        if(pv>=pe) {  // there is a key but no value
          if(len>0 && pk[len-1]=='=') len--;
          sprintf(pair[pairn].k,"<tag k=\"%.*s\"",len,pk);
            // assemble the search string for the key
          pair[pairn].klen= 9+len;  // recalculate key length
          pair[pairn].v[0]= 0;  // null string because there is no value
          }
        else {  // key and value
          sprintf(pair[pairn].k,"<tag k=\"%.*s\" v=\"",len,pk);
            // assemble the search string for the key
          pair[pairn].klen= 13+len;  // recalculate key length
          if(pairn>0 && 
              (len==0 || strcmp(pair[pairn].k,pair[pairn-1].k)==0)) {
              // no key or same key as previous one
            pair[pairn].k[0]= 0;  // mark pair as 'pair with same key'
            pair[pairn].klen= 0;  // mark key length as
              // 'same key key length as previous one'
            }
          len= pe-pv-1;  // length of this value
          if(len>(pairkMAX-20)) len= pairkMAX-20;
            // delimit value length
          sprintf(pair[pairn].v,"%.*s\"",len,pv+1);
            // assemble the search string for the value
          }
        pairn++;  // next pair in key/val table
        pk= pe;  // jump to next key/val pair in parameter list
        }  // end   for every key/val pair
      }  // end   for every parameter in command line
    if(h_n==0) h_n= 400;  // use standard value if not set otherwise
    if(h_w==0 && h_r==0) {
        // user chose simple form for hash memory value
      // take the one given value as reference and determine the 
      // three values using these factors: 80%, 15%, 5%
      h_w= h_n/5; h_r= h_n/20;
      h_n-= h_w; h_w-= h_r; }
    if(testmode) {
        // user wants a print-out of the command line parameters only
      int i;

      for(i= 0;i<pairn;i++)  // for every pair in list
        printf("pair %i (key;keylen;val): %s;%i;%s\n",
          i,pair[i].k,pair[i].klen,pair[i].v);
      printf("hash size (node;way;relation): %i;%i;%i\n",h_n,h_w,h_r);
      if(dropsect) {  // if pre-filtering was requested
        printf("Dropping: ");
        if(keydrop[1]) printf("nodes ");
        if(keydrop[2]) printf("ways ");
        if(keydrop[3]) printf("relations");
        if(keydrop[4]) printf("changesets");
        printf("\n");
        }
return 0;  // end the program, because there shall be no processing
      }  // end   user wants a print-out of the command line parameters
    }  // end   read command line parameters

  // initializations
  insidepreproc= 0;
  insidequote= false;
  bufp= buf;
  nesting= 0;
  preserve= dropsect;
  drop= false;
  justdropped= false;

  for(;;) {  // main loop
    r= readbyte(&c);  // read a single byte, use internal buffer
    if(justdropped) {  // the last section has been dropped
      justdropped= false;
      // jump over all trailing whitespaces
      while(r>0 && (c==' ' || c=='\t' || c=='\r' || c=='\n'))
        r= readbyte(&c);  // read next byte
      }
    if(r<0)  // we reached the end of the file
  break;  // end processing
    keybufp= keybuf;  // default: we've read no key identifier
    if(c=='\"')  // we encountered a quotation mark
      insidequote= !insidequote;
    else if(!insidequote) {  // we're not in a quoted area
      if(c=='<') {  // a key identifier starts here
        // read-in the key identifier
        keybuf[0]= c;
        keybufp= keybuf+1;  // key starts after the '<'
        keybuft= NULL;
        for(;;) {  // for every character of the key identifier
          r= readbyte(&c);  // read one character from stdin
          if(r<=0 || c=='>' || keybufp>=keybufee)
              // no more characters of the key identifier to read
        break;
          if(keybuft==NULL && (c==' ' || c=='>')) keybuft= keybufp;
            // store the memory address of the key's end
          *keybufp++= c;  // store next character of the key
          }  // end   for every character of the key identifier
        if(keybuft==NULL) keybuft= keybufp;  // if not already
          // having been set: set the address of the key's end
        *keybufp= 0;  // add null-terminator to key identifier
        // process the key
        if(nesting>0) {  // we're already inside a specified section
          bool kfound;  // we did find one of the searched keys

          kfound= false;
          if(!drop && !preserve) {
              // neither this section is to be dropped nor
              // is it already listed to being preserved
#if 0 ///
            // test for searched IDs
            if(insidepreproc>0) {
                // there is (or has been) preprocessing

              if(parse_hashget(keybuf)) {
                  // present key identifier had been listed
                  // to being preserved
                kfound= true;
                preserve= true;
                }
              }
#endif
            // test for searched pair
            if(!preserve) {
                // section is not already listed to being preserved;
              int i;  // for index of the key in search string list

              for(i= 0;i<pairn;i++) {
                  // for every key in search string list
                if(pair[i].klen==0 ||
                    memcmp(keybuf,pair[i].k,pair[i].klen)!=0)
                    // same as previous key OR
                    // key does not fit to examined string
                  continue;  // look for next key in list
                // here: we found one of the searched keys
                kfound= true;
                keybuft= keybuf+pair[i].klen;  // jump over the key
                for(;;) {  // for every value of this key
                  if(pair[i].v[0]==0 || strzcmp(keybuft,pair[i].v)==0) {
                      // we don't need to care about the value OR
                      // examined string fits to the value
                    preserve= true;
                    goto foundthepair;  // escape both loops
                    }  // end   for every value of this key
                  if(i+1>=pairn || pair[i+1].klen>0)
                      // next key is different
                break;
                  // here: next key is identical to the present key
                  i++;  // step to next key
                  }  // end   for every value of this key
                }  // end   for every key in search string list
              foundthepair:;
              }  // end   for every key in search string list
            }  // end   test for searched pair
          if(!drop && strcmp(keybuf,"<preserveall/")==0)
              // section is not to drop completely AND
              // we encountered a special "preserve key"
            preserve= true;  // only for test purposes
          else if(!drop && kfound) {
            // decide if we preserve this section
            // (we already decided it some lines above and did set 'preserve')
            }
          else if(keybuft>keybuf+1 &&
              memcmp(keybuf+1,keyp,keybuft-keybuf-1)==0 &&
              keyp[keybuft-keybuf]==0)  // same key as previous key
              // is now starting a nested section
            nesting++;  // memorize that we dived one layer deeper
          else if(keybuft>keybuf+2 && keybuf[1]=='/' &&
              memcmp(keybuf+2,keyp,keybuft-keybuf-2)==0 &&
              keyp[keybuft-keybuf]==0) {  // same key as previous key
              // is now ending the section
            nesting--;  // memorize that went up one layer
            if(nesting==0) {  // outermost section has ended
              if(!preserve) {
                  // nobody found the section worth keeping
                drop= true;  // make sure to drop the section
                }
              preserve= dropsect;  // initialize the variable again
              }  // end   outermost section has ended
            }  // end   same key as prev. key is now ending the section
          }  // end   we're already inside a specified section
        else if(keybuft>keybuf+1 && keybuf[1]!='/') {
            // maybe a new specified section starts here
          // note: in this 'else' branch we are on nesting level 0;
          // now, find out if this section starts with one of
          // the specified keys
          keyi= 0;  // first index of keys in drop list
          keyp= keys[0];  // first key in list
          while(keyp[0]!=0) {  // for every key in list
            int len;

            len= keybuft-keybuf-1;
            if(len>0 && keybuf[len]=='/') {
              len--;
              while(len>0 && keybuf[len]==' ') len--;
              }
            if(memcmp(keybuf+1,keyp,len)==0 &&
                keyp[keybuft-keybuf]==0)
                // examined key is identical to key in list
          break;  // the section's key is listed
            keyi++; keyp+= sizeof(keys[0]);  // next key in list
            }
          if(keyp[0]!=0) {  // yes it's a specified key
            nesting= 1;  // we entered a specified section
            drop= keydrop[keyi];
              // set drop mark in case this section is
              // to be dropped regardless of its contents
            if(keyi==0) {  // this section is the preprocessor tag
              if(insidepreproc==0) {  // it's the first preprocessor tag
                int i;

                insidepreproc= 1;  
                  // mark that we're now in the phase of preprocessing
                i= hash_ini(h_n,h_w,h_r);  // initialize hash table
                if(i==1) write(2,
                  "Warning: hash size had to be reduced.\n",38);
                else if(i==2) write(2,
                  "Error: not enough memory.\n",26);
                }
              else if(insidepreproc==1)
                  // presently, we are in the phase of preprocessing
                insidepreproc= 2;
                  // mark that we just left the phase preprocessing
              }  // end   preprocessor tag
            if(keybufp[-1]=='/') {  // section ends right after start,
                // so it is a small section
              if(!dropsect)  // not running a prefiltering
                drop= true;  // in regular processing small sections
                  // will be node sections without tags and therefore
                  // shall be deleted unless listed in hash table
              nesting= 0;  // we just left a small outermost section
              }  // end   it is a small section
            // test for searched IDs
            if(!dropsect) {  // not running a prefiltering
              if(insidepreproc>0) {
                  // there is (or has been) preprocessing
                if(parse_hashget(keybuf)) {
                    // present key identifier had been listed
                    // to being preserved
                  drop= false;
                  preserve= true;
                    // this small section shall be preserved
                  }
                }
              }  // end   not a run of prefiltering AND
            }  // end   yes it's a specified key
          }  // end   maybe a new specified section starts here
        }  // end   a key identifier starts here
      }  // end   we're not in a quoted area
    if(nesting<=0) {  // we're not inside a specified section
      if(drop) {  // previously processed section shall be dropped
        // skip the buffered section
        drop= false;
        bufp= buf;  // clear buffer
        keybufp= keybuf;  // clear key buffer
        justdropped= true;  // memorize that trailing spaces and
          // newlines are to be dropped as well
        }  // end   last section shall be dropped
      else {  // last section must not be dropped
        if(bufp>buf) {  // there are some characters in
            // buffer waiting to be written
          if(insidepreproc==1) {  // inside phase of preprocessing
            *bufp= 0;  // set null-terminator
            parse_hashset(buf);  // parse for key identifiers
            }
          else  // not inside phase of preprocessing
            writestdout(buf,bufp-buf);  // write buffer contents
          bufp= buf;  // clear buffer
          }
        if(keybufp>keybuf) {  // there are some characters in
            // key buffer waiting to be written
          if(insidepreproc==1) {  // inside phase of preprocessing
            *keybufp= 0;  // set null-terminator
            parse_hashset(keybuf);  // parse for key identifiers
            }
          else  // not inside phase of preprocessing
            writestdout(keybuf,keybufp-keybuf);
              // write key buffer contents
          }
        if(r>=0 && insidepreproc!=1)  // read-in character is valid
          writestdout(&c,1);  // write the read-in character to stdout
        }  // end   last section must not be dropped
      }  // end   we're not inside a specified section
    else if(!drop) {
        // we are inside a specified section which is not to drop
      if((keybufp-keybuf+1)>(bufe-bufp))  // buffer too small
        drop= true;  // we decide to drop this section because it's too
          // large and therefore cannot be processed by this program
      else {  // buffer size is sufficient
        // store all data of the key we've read
        if(keybufp>keybuf) {  // at least one character has been read
          memcpy(bufp,keybuf,keybufp-keybuf);
            // copy data from key buffer to normal buffer
          bufp+= keybufp-keybuf;  // increase buffer pointer accordingly
          }
        if(r>=0)  // read-in character is valid
          *bufp++= c;  // add character to the buffer
        }  // end   buffer size is sufficient
      }  // end   we are inside a specified section which is not to drop
    }  // end   main loop
  writestdout(&c,-1);  // flush write buffer
  return 0;
  }  // end   main()

