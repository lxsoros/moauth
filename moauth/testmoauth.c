/*
 * Unit test program for moauth library
 *
 * Copyright © 2017-2018 by Michael R Sweet
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include "moauth-private.h"


/*
 * 'main()' - Main entry for unit test program.
 */

int					/* O - Exit status */
main(void)
{
  int           status = 0;             /* Exit status */
  int           i;                      /* Looping var */
  char          *data;                  /* Data pointer */
  int           count,                  /* Expected variable count */
                num_vars;               /* Number of variables */
  cups_option_t *vars;                  /* Variables */
  const char    *value;                 /* Value */
  static const char * const decodes[] = /* Decode string tests */
  {
    "0?",
    "0?name",
    "0?%00=",
    "1?empty=",
    "1?name=value",
    "0?name=value&",
    "2?name1=value1&name2=value2",
    "1?name+with+spaces=value+with+spaces",
    "1?quotes=%22value%22"
  };
  static const char * const encodes[][3] =
  {                                     /* Encode string tests */
    { "empty", "", "empty=" },
    { "name", "value", "empty=&name=value" },
    { "name", "value with spaces", "empty=&name=value+with+spaces" },
    { "quotes", "\"value\"", "empty=&name=value+with+spaces&quotes=%22value%22" },
    { "equation", "1+2=3 & 2+1=3", "empty=&equation=1%2B2%3D3+%26+2%2B1%3D3&name=value+with+spaces&quotes=%22value%22" }
  };
  static const char *json_in =
    "{\n"
    "\"access_token\":\"2YotnFZFEjr1zCsicMWpAA\",\n"
    "\"example_array\":[\"value1\",\"value2\",\"value3\"],\n"
    "\"example_parameter\":\"example_value\",\n"
    "\"expires_in\":3600,\n"
    "\"refresh_token\":\"tGzv3JOkF0XG5Qx2TlKWIA\",\n"
    "\"token_type\":\"example\"\n"
    "}\n";
  static const char *json_out =
    "{"
    "\"access_token\":\"2YotnFZFEjr1zCsicMWpAA\","
    "\"example_array\":[\"value1\",\"value2\",\"value3\"],\n"
    "\"example_parameter\":\"example_value\","
    "\"expires_in\":3600,"
    "\"refresh_token\":\"tGzv3JOkF0XG5Qx2TlKWIA\","
    "\"token_type\":\"example\""
    "}";


 /*
  * Test decoding different URL strings...
  */

  for (i = 0; i < (int)(sizeof(decodes) / sizeof(decodes[0])); i ++)
  {
    printf("_moauthFormDecode(\"%s\", ...): ", decodes[i]);

    count    = (int)strtol(decodes[i], NULL, 10);
    num_vars = _moauthFormDecode(decodes[i] + 2, &vars);

    if (count != num_vars)
    {
      printf("FAIL (got %d variables, expected %d)\n", num_vars, count);
      status = 1;
    }
    else if ((value = cupsGetOption("empty", num_vars, vars)) != NULL && strcmp(value, ""))
    {
      printf("FAIL (got \"empty=%s\", expected \"empty=\")\n", value);
      status = 1;
    }
    else if ((value = cupsGetOption("name", num_vars, vars)) != NULL && strcmp(value, "value"))
    {
      printf("FAIL (got \"name=%s\", expected \"name=value\")\n", value);
      status = 1;
    }
    else if ((value = cupsGetOption("name1", num_vars, vars)) != NULL && strcmp(value, "value1"))
    {
      printf("FAIL (got \"name1=%s\", expected \"name1=value1\")\n", value);
      status = 1;
    }
    else if ((value = cupsGetOption("name2", num_vars, vars)) != NULL && strcmp(value, "value2"))
    {
      printf("FAIL (got \"name2=%s\", expected \"name2=value2\")\n", value);
      status = 1;
    }
    else if ((value = cupsGetOption("name with spaces", num_vars, vars)) != NULL && strcmp(value, "value with spaces"))
    {
      printf("FAIL (got \"name with spaces=%s\", expected \"name with spaces=value with spaces\")\n", value);
      status = 1;
    }
    else if ((value = cupsGetOption("quotes", num_vars, vars)) != NULL && strcmp(value, "\"value\""))
    {
      printf("FAIL (got \"quotes=%s\", expected \"quotes=\\\"value\\\"\")\n", value);
      status = 1;
    }
    else
      puts("PASS");

    cupsFreeOptions(num_vars, vars);
  }

 /*
  * Test encoding different form variables...
  */

  for (i = 0, num_vars = 0, vars = NULL; i < (int)(sizeof(encodes) / sizeof(encodes[0])); i ++)
  {
    printf("_moauthFormEncode(\"%s=%s\", ...): ", encodes[i][0], encodes[i][1]);

    num_vars = cupsAddOption(encodes[i][0], encodes[i][1], num_vars, &vars);
    data     = _moauthFormEncode(num_vars, vars);

    if (!data)
    {
      puts("FAIL (unable to encode)");
      status = 1;
    }
    else if (strcmp(data, encodes[i][2]))
    {
      printf("FAIL (got \"%s\", expected \"%s\")\n", data, encodes[i][2]);
      status = 1;
    }
    else
      puts("PASS");

    if (data)
      free(data);
  }

  cupsFreeOptions(num_vars, vars);

 /*
  * Test JSON encoding/decoding...
  */

  fputs("_moauthJSONDecode(...): ", stdout);

  num_vars = _moauthJSONDecode(json_in, &vars);

  if (num_vars != 6)
  {
    printf("FAIL (got %d vars, expected 6)\n", num_vars);
    status = 1;
  }
  else if ((value = cupsGetOption("access_token", num_vars, vars)) == NULL || strcmp(value, "2YotnFZFEjr1zCsicMWpAA"))
  {
    if (value)
      printf("FAIL (got \"%s\" for \"access_token\", expected \"2YotnFZFEjr1zCsicMWpAA\")\n", value);
    else
      puts("FAIL (missing \"access_token\")");

    status = 1;
  }
  else if ((value = cupsGetOption("example_array", num_vars, vars)) == NULL || strcmp(value, "[\"value1\",\"value2\",\"value3\"]"))
  {
    if (value)
      printf("FAIL (got \"%s\" for \"example_array\", expected '[\"value1\",\"value2\",\"value3\"]')\n", value);
    else
      puts("FAIL (missing \"example_array\")");

    status = 1;
  }
  else if ((value = cupsGetOption("example_parameter", num_vars, vars)) == NULL || strcmp(value, "example_value"))
  {
    if (value)
      printf("FAIL (got \"%s\" for \"example_parameter\", expected \"example_value\")\n", value);
    else
      puts("FAIL (missing \"example_parameter\")");

    status = 1;
  }
  else if ((value = cupsGetOption("expires_in", num_vars, vars)) == NULL || strcmp(value, "3600"))
  {
    if (value)
      printf("FAIL (got \"%s\" for \"expires_in\", expected \"3600\")\n", value);
    else
      puts("FAIL (missing \"expires_in\")");

    status = 1;
  }
  else if ((value = cupsGetOption("refresh_token", num_vars, vars)) == NULL || strcmp(value, "tGzv3JOkF0XG5Qx2TlKWIA"))
  {
    if (value)
      printf("FAIL (got \"%s\" for \"refresh_token\", expected \"tGzv3JOkF0XG5Qx2TlKWIA\")\n", value);
    else
      puts("FAIL (missing \"refresh_token\")");

    status = 1;
  }
  else if ((value = cupsGetOption("token_type", num_vars, vars)) == NULL || strcmp(value, "example"))
  {
    if (value)
      printf("FAIL (got \"%s\" for \"token_type\", expected \"example\")\n", value);
    else
      puts("FAIL (missing \"token_type\")");

    status = 1;
  }
  else
    puts("PASS");

  fputs("_moauthJSONEncode(...): ", stdout);

  if ((data = _moauthJSONEncode(num_vars, vars)) == NULL)
  {
    puts("FAIL (unable to encode)");
    status = 1;
  }
  else if (strcmp(data, json_out))
  {
    printf("FAIL (got \"%s\", expected \"%s\")\n", data, json_out);
    status = 1;
  }
  else
    puts("PASS");

  if (data)
    free(data);

  cupsFreeOptions(num_vars, vars);

 /*
  * Return the test results...
  */

  return (status);
}
