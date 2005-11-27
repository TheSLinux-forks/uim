/*
  Copyright (c) 2005, Konosuke Watanabe <nosuke@csc.ne.jp>
  All rights reserved.

  Redistribution and use in source and binary forms, with or
  without modification, are permitted provided that the
  following conditions are met:

  1. Redistributions of source code must retain the above
     copyright notice, this list of conditions and the
     following disclaimer.
  2. Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the
     following disclaimer in the documentation and/or other
     materials provided with the distribution.
  3. Neither the name of authors nor the names of its
     contributors may be used to endorse or promote products
     derived from this software without specific prior written
     permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "im.h"

/* context's default value */
char *default_engine_name = NULL;

int default_engine_updated = 0;

/*
int
init_default_engine(void)
{
  if (default_engine_name) free(default_engine_name);

  default_engine_name 
	= strdup(uim_get_default_im_name(setlocale(LC_ALL, NULL)));

  return 1;
}
*/

void
update_default_engine(const char *engine_name)
{
  if (default_engine_name) free(default_engine_name);
  default_engine_name = strdup(engine_name);

  default_engine_updated = 1;
}

void
output_default_im_engine(void)
{
  if (default_engine_name)
	a_printf(" ( d \"%s\" ) ", default_engine_name);
}


int
check_im_name(const char *imname)
{
  int ret = 0, i;
  const char *name;
  uim_context context;

  context = uim_create_context(NULL, "UTF-8", NULL, NULL, NULL, NULL);

  for (i = 0; i < uim_get_nr_im(context); i++) {  
	name = uim_get_im_name(context, i);
	if (strcmp(name, imname) == 0) ret = 1;
  }

  uim_release_context(context);

  return ret;
}




/* show supported IM engines */
int
list_im_engine(void)
{
  int i;

  uim_context context;

  context = uim_create_context(NULL, "UTF-8", NULL, NULL, NULL, NULL);

  /*
  if (default_engine_name)
	a_printf(" ( L \"%s\" ", default_engine_name);
  else
	a_printf(" ( L \"%s\" ", uim_get_default_im_name(setlocale(LC_ALL, NULL)));
  */
  /*a_printf(" ( L "); */

  a_printf(" ( L \"%s\" ", uim_get_default_im_name(setlocale(LC_ALL, NULL)));
  
  for (i = 0 ; i < uim_get_nr_im(context); i++) {
	char dummy_str[] = "";
	const char *name, *lang, *language, *shortd, *encoding;

	if ((name = uim_get_im_name(context, i)) == NULL)
	  name = dummy_str;

	if ((lang = uim_get_im_language(context, i)) == NULL)
	  lang = dummy_str;

	if ((language = uim_get_language_name_from_locale(lang)) == NULL)
	  language = dummy_str;

	if ((shortd = uim_get_im_short_desc(context, i)) == NULL)
	  shortd = dummy_str;
	
	a_printf(" ( \"%s\" \"%s\" \"%s\" \"%s\" ",
			 name, lang, language, shortd);

	if ((encoding = uim_get_im_encoding(context, i)) == NULL)
	  a_printf(" %s ) ", encoding);
	else
	  a_printf(" UTF-8 ) "); /* or nil? */
  }

  a_printf(" ) ");

  uim_release_context(context);

  return i;
}


