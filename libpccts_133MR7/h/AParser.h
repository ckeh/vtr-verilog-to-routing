/* ANTLRParser.h
 *
 * Define the generic ANTLRParser superclass, which is subclassed to
 * define an actual parser.
 *
 * Before entry into this file: ANTLRTokenType must be set.
 *
 * SOFTWARE RIGHTS
 *
 * We reserve no LEGAL rights to the Purdue Compiler Construction Tool
 * Set (PCCTS) -- PCCTS is in the public domain.  An individual or
 * company may do whatever they wish with source code distributed with
 * PCCTS or the code generated by PCCTS, including the incorporation of
 * PCCTS, or its output, into commerical software.
 *
 * We encourage users to develop software with PCCTS.  However, we do ask
 * that credit is given to us for developing PCCTS.  By "credit",
 * we mean that if you incorporate our source code into one of your
 * programs (commercial product, research project, or otherwise) that you
 * acknowledge this fact somewhere in the documentation, research report,
 * etc...  If you like PCCTS and have developed a nice tool with the
 * output, please mention that you developed it using PCCTS.  In
 * addition, we ask that this header remain intact in our source code.
 * As long as these guidelines are kept, we expect to continue enhancing
 * this system and expect to make other tools available as they are
 * completed.
 *
 * ANTLR 1.33
 * Terence Parr
 * Parr Research Corporation
 * with Purdue University and AHPCRC, University of Minnesota
 * 1989-1995
 */

#ifndef APARSER_H_GATE
#define APARSER_H_GATE

#include <stdio.h>
#include <setjmp.h>
#include "config.h"
#include ATOKEN_H
#include ATOKENBUFFER_H

#ifdef ZZCAN_GUESS
#ifndef ZZINF_LOOK
#define ZZINF_LOOK
#endif
#endif


#define NLA			(token_type[lap&(LLk-1)])/* --> next LA */

typedef unsigned char SetWordType;

/* Define external bit set stuff (for SetWordType) */
#define EXT_WORDSIZE	(sizeof(char)*8)
#define EXT_LOGWORDSIZE	3

           /* s y n t a c t i c  p r e d i c a t e  s t u f f */

typedef struct _zzjmp_buf {
			jmp_buf state;
		} zzjmp_buf;

/* these need to be macros not member functions */
#define zzGUESS_BLOCK		ANTLRParserState zzst; int zzrv; int _marker;
#define zzNON_GUESS_MODE	if ( !guessing )
#define zzGUESS_FAIL		guess_fail();
#define zzGUESS_DONE		{zzrv=1; inputTokens->rewind(_marker); guess_done(&zzst);}
#define zzGUESS				saveState(&zzst); \
							guessing = 1; \
							_marker = inputTokens->mark(); \
							zzrv = setjmp(guess_start.state); \
						    if ( zzrv ) zzGUESS_DONE

                  /* a n t l r  p a r s e r  d e f */

struct ANTLRParserState {
	/* class variables */
	zzjmp_buf guess_start;
	int guessing;

	int inf_labase;
	int inf_last;

	int dirty;
};

/* notes:
 *
 * multiple inheritance is a cool way to include what stuff is needed
 * in this structure (like guess stuff).  however, i'm not convinced that
 * multiple inheritance works correctly on all platforms.  not that
 * much space is used--just include all possibly useful members.
 *
 * the class should also be a template with arguments for the lookahead
 * depth and so on.  that way, more than one parser can be defined (as
 * each will probably have different lookahead requirements).  however,
 * am i sure that templates work?  no, i'm not sure.
 *
 * no attributes are maintained and, hence, the 'asp' variable is not
 * needed.  $i can still be referenced, but it refers to the token
 * associated with that rule element.  question: where are the token's
 * stored if not on the software stack?  in local variables created
 * and assigned to by antlr.
 */
class ANTLRParser {
protected:
	/* class variables */
	static SetWordType bitmask[sizeof(SetWordType)*8];
	static char eMsgBuffer[500];

protected:
	int LLk;					// number of lookahead symbols (old LL_K)
	int demand_look;
	ANTLRTokenType eofToken;			// when do I stop during resynch()s
	int bsetsize;				// size of bitsets created by ANTLR in
								// units of SetWordType

	ANTLRTokenBuffer *inputTokens;	//place to get input tokens

	zzjmp_buf guess_start;		// where to jump back to upon failure
	int guessing;				// if guessing (using (...)? predicate)

	// infinite lookahead stuff
	int can_use_inf_look;		// set by subclass (generated by ANTLR)
	int inf_lap;
	int inf_labase;
	int inf_last;
	int *_inf_line;

	ANTLRChar **token_tbl;		// pointer to table of token type strings

	int dirty;					// used during demand lookahead

	ANTLRTokenType *token_type;		// fast reference cache of token.getType()
//	ANTLRLightweightToken **token;	// the token with all its attributes
	int lap;
	int labase;

private:
	void fill_inf_look();

protected:
	void guess_fail()				{ longjmp(guess_start.state, 1); }
	void guess_done(ANTLRParserState *st){ restoreState(st); }
	int guess(ANTLRParserState *);
	void look(int);
    int _match(ANTLRTokenType, ANTLRChar **, ANTLRTokenType *,
			   _ANTLRTokenPtr *, SetWordType **);
    int _setmatch(SetWordType *, ANTLRChar **, ANTLRTokenType *,
			   _ANTLRTokenPtr *, SetWordType **);
    int _match_wsig(ANTLRTokenType);
    int _setmatch_wsig(SetWordType *);
    virtual void consume();
    void resynch(SetWordType *wd,SetWordType mask);
	void prime_lookahead();
	virtual void tracein(char *r)
			{
				fprintf(stderr, "enter rule \"%s\"\n", r);
			}
	virtual void traceout(char *r)
			{
				fprintf(stderr, "exit rule \"%s\"\n", r);
			}
	unsigned MODWORD(unsigned x) {return x & (EXT_WORDSIZE-1);}	// x % EXT_WORDSIZE
	unsigned DIVWORD(unsigned x) {return x >> EXT_LOGWORDSIZE;}	// x / EXT_WORDSIZE
	int set_deg(SetWordType *);
	int set_el(ANTLRTokenType, SetWordType *);
	virtual void edecode(SetWordType *);				// MR1
	virtual void FAIL(int k, ...);					// MR1

public:
	ANTLRParser(ANTLRTokenBuffer *,
				int k=1,
				int use_inf_look=0,
				int demand_look=0,
				int bsetsize=1);
	virtual ~ANTLRParser();

	virtual void init();
	
	ANTLRTokenType LA(int i)
	{
		return demand_look ? token_type[(labase+(i)-1)&(LLk-1)] :
							token_type[(lap+(i)-1)&(LLk-1)];
	}
	_ANTLRTokenPtr LT(int i);

	void setEofToken(ANTLRTokenType t)	{ eofToken = t; }

	void noGarbageCollectTokens()	{ inputTokens->noGarbageCollectTokens(); }
	void garbageCollectTokens()		{ inputTokens->garbageCollectTokens(); }

        virtual void syn(_ANTLRTokenPtr tok, ANTLRChar *egroup,
					 SetWordType *eset, ANTLRTokenType etok, int k);
	void saveState(ANTLRParserState *);
	void restoreState(ANTLRParserState *);

	virtual void panic(char *msg);
	static char *eMsgd(char *,int);
	static char *eMsg(char *,char *);
	static char *eMsg2(char *,char *,char *);

	void consumeUntil(SetWordType *st);
	void consumeUntilToken(int t);

	virtual int _setmatch_wdfltsig(SetWordType *tokensWanted,
					 ANTLRTokenType tokenTypeOfSet,
					 SetWordType *whatFollows);
	virtual int _match_wdfltsig(ANTLRTokenType tokenWanted,
					 SetWordType *whatFollows);
	
	const ANTLRChar * parserTokenName(int tok);			// MR1
};


#define zzmatch(_t)							\
	if ( !_match((ANTLRTokenType)_t, &zzMissText, &zzMissTok, \
				 (_ANTLRTokenPtr *) &zzBadTok, &zzMissSet) ) goto fail;

#define zzmatch_wsig(_t,handler)						\
	if ( !_match_wsig((ANTLRTokenType)_t) ) if ( guessing ) goto fail; else {_signal=MismatchedToken; goto handler;}

#define zzsetmatch(_ts)							\
	if ( !_setmatch(_ts, &zzMissText, &zzMissTok, \
				 (_ANTLRTokenPtr *) &zzBadTok, &zzMissSet) ) goto fail;

#define zzsetmatch_wsig(_ts, handler)				\
	if ( !_setmatch_wsig(_ts) ) if ( guessing ) goto fail; else {_signal=MismatchedToken; goto handler;}

/* For the dflt signal matchers, a FALSE indicates that an error occurred
 * just like the other matchers, but in this case, the routine has already
 * recovered--we do NOT want to consume another token.  However, when
 * the match was successful, we do want to consume hence _signal=0 so that
 * a token is consumed by the "if (!_signal) consume(); _signal=NoSignal;"
 * preamble.
 */
#define zzsetmatch_wdfltsig(tokensWanted, tokenTypeOfSet, whatFollows) \
	if ( !_setmatch_wdfltsig(tokensWanted, tokenTypeOfSet, whatFollows) ) \
		_signal = MismatchedToken;

#define zzmatch_wdfltsig(tokenWanted, whatFollows) \
	if ( !_match_wdfltsig(tokenWanted, whatFollows) ) _signal = MismatchedToken;


//  MR1  10-Apr-97 	zzfailed_pred() macro does not backtrack
//  MR1			  in guess mode.
//  MR1			Identification and correction due to J. Lilley

#ifndef zzfailed_pred
#define zzfailed_pred(_p) \
  if (guessing) { \
    zzGUESS_FAIL; \
  } else { \
    fprintf(stdout,"line %d: semantic error; failed predicate: '%s'\n", \
	LT(1)->getLine(), _p); \
  }
#endif

#define zzRULE \
		SetWordType *zzMissSet=NULL; ANTLRTokenType zzMissTok=(ANTLRTokenType)0;	\
		_ANTLRTokenPtr zzBadTok; ANTLRChar *zzBadText=(ANTLRChar *)"";	\
		int zzErrk=1;									\
		ANTLRChar *zzMissText=(ANTLRChar *)"";

#endif

        /* S t a n d a r d  E x c e p t i o n  S i g n a l s */

#define NoSignal			0
#define MismatchedToken		1
#define NoViableAlt			2
#define NoSemViableAlt		3

/* MR7  Allow more control over signalling                                  */
/*        by adding "Unwind" and "SetSignal"                                */

#define Unwind              4
#define setSignal(newValue) *_retsignal=_signal=(newValue)
#define suppressSignal       *_retsignal=_signal=0
#define exportSignal        *_retsignal=_signal