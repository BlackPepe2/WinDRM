/******************************************************************************\
 * Copyright (c) 2004
 *
 * Author(s):
 *	Francesco Lanza
 *
 * Description:
 *	
 *
 ******************************************************************************/

/*--------------------------------------------------------------------

        callsign.h

  --------------------------------------------------------------------*/

char * getcall();
void setcall(const char * thecall);

BOOL callisok(void);
BOOL pathisok(void);

int calcpacklen(int iNoBits);


