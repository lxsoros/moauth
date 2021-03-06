/*
 * Token grant/introspection support for moauth library
 *
 * Copyright © 2017-2019 by Michael R Sweet
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#include <config.h>
#include "moauth-private.h"


/*
 * 'moauthGetToken()' - Get an access token from a grant from the OAuth server.
 */

char *					/* O - Access token or @code NULL@ on error */
moauthGetToken(
    moauth_t   *server,			/* I - Connection to OAuth server */
    const char *redirect_uri,		/* I - Redirection URI that was used */
    const char *client_id,		/* I - Client ID that was used */
    const char *grant,			/* I - Grant code */
    const char *code_verifier,		/* I - Code verifier string, if any */
    char       *token,			/* I - Access token buffer */
    size_t     tokensize,		/* I - Size of access token buffer */
    char       *refresh,		/* I - Refresh token buffer */
    size_t     refreshsize,		/* I - Size of refresh token buffer */
    time_t     *expires)		/* O - Expiration date/time, if known */
{
  http_t	*http = NULL;		/* HTTP connection */
  char		resource[256];		/* Token endpoint resource */
  http_status_t	status;			/* Response status */
  int		num_form = 0;		/* Number of form variables */
  cups_option_t	*form = NULL;		/* Form variables */
  char		*form_data = NULL;	/* POST form data */
  size_t	form_length;		/* Length of data */
  char		*json_data = NULL;	/* JSON response data */
  int		num_json = 0;		/* Number of JSON variables */
  cups_option_t	*json = NULL;		/* JSON variables */
  const char	*value;			/* JSON value */


 /*
  * Range check input...
  */

  if (token)
    *token = '\0';

  if (refresh)
    *refresh = '\0';

  if (expires)
    *expires = 0;

  if (!server || !redirect_uri || !client_id || !grant || !token || tokensize < 32)
  {
    if (server)
      snprintf(server->error, sizeof(server->error), "Bad arguments to function.");

    return (NULL);
  }

  if (!server->token_endpoint)
  {
    snprintf(server->error, sizeof(server->error), "Authorization not supported.");
    return (0);
  }

 /*
  * Prepare form data to get an access token...
  */

  num_form = cupsAddOption("grant_type", "authorization_code", num_form, &form);
  num_form = cupsAddOption("code", grant, num_form, &form);
  num_form = cupsAddOption("redirect_uri", redirect_uri, num_form, &form);
  num_form = cupsAddOption("client_id", client_id, num_form, &form);

  if (code_verifier)
    num_form = cupsAddOption("code_verifier", code_verifier, num_form, &form);

  if ((form_data = _moauthFormEncode(num_form, form)) == NULL)
  {
    snprintf(server->error, sizeof(server->error), "Unable to encode form data.");
    goto done;
  }

  form_length = strlen(form_data);

 /*
  * Send a POST request with the form data...
  */

  if ((http = _moauthConnect(server->token_endpoint, resource, sizeof(resource))) == NULL)
  {
    snprintf(server->error, sizeof(server->error), "Connection to token endpoint failed: %s", cupsLastErrorString());
    goto done;
  }

  httpClearFields(http);
  httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/x-www-form-urlencoded");
  httpSetLength(http, form_length);

  if (httpPost(http, resource))
  {
    if (httpReconnect2(http, 30000, NULL))
    {
      snprintf(server->error, sizeof(server->error), "Reconnect failed: %s", cupsLastErrorString());
      goto done;
    }

    if (httpPost(http, resource))
    {
      snprintf(server->error, sizeof(server->error), "POST failed: %s", cupsLastErrorString());
      goto done;
    }
  }

  if (httpWrite2(http, form_data, form_length) < form_length)
  {
    snprintf(server->error, sizeof(server->error), "Write failed: %s", cupsLastErrorString());
    goto done;
  }

  while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

  if (status == HTTP_STATUS_OK)
  {
    json_data = _moauthCopyMessageBody(http);
    num_json  = _moauthJSONDecode(json_data, &json);

    if ((value = cupsGetOption("access_token", num_json, json)) != NULL)
    {
      strncpy(token, value, tokensize - 1);
      token[tokensize - 1] = '\0';
    }

    if (expires && (value = cupsGetOption("expires_in", num_json, json)) != NULL)
      *expires = time(NULL) + atoi(value);

    if (refresh && (value = cupsGetOption("refresh_token", num_json, json)) != NULL)
    {
      strncpy(refresh, value, refreshsize - 1);
      refresh[refreshsize - 1] = '\0';
    }
  }
  else
  {
    snprintf(server->error, sizeof(server->error), "Unable to get access token: POST status %d", status);
  }

 /*
  * Return whatever we got...
  */

  done:

  httpClose(http);

  cupsFreeOptions(num_form, form);
  if (form_data)
    free(form_data);

  cupsFreeOptions(num_json, json);
  if (json_data)
    free(json_data);

  return (*token ? token : NULL);
}


/*
 * 'moauthIntrospectToken()' - Get information about an access token.
 */

int					/* O - 1 if the token is active, 0 otherwise */
moauthIntrospectToken(
    moauth_t   *server,			/* I - Connection to OAuth server */
    const char *token,			/* I - Access token */
    char       *username,		/* I - Username buffer */
    size_t     username_size,		/* I - Size of username string */
    char       *scope,			/* I - Scope buffer */
    size_t     scope_size,		/* I - Size of scope string */
    time_t     *expires)		/* O - Expiration date */
{
  http_t	*http = NULL;		/* HTTP connection */
  char		resource[256];		/* Token endpoint resource */
  http_status_t	status;			/* Response status */
  int		num_form = 0;		/* Number of form variables */
  cups_option_t	*form = NULL;		/* Form variables */
  char		*form_data = NULL;	/* POST form data */
  size_t	form_length;		/* Length of data */
  char		*json_data = NULL;	/* JSON response data */
  int		num_json = 0;		/* Number of JSON variables */
  cups_option_t	*json = NULL;		/* JSON variables */
  const char	*value;			/* JSON value */
  int		active = 0;		/* Is the token active? */


 /*
  * Range check input...
  */

  if (username)
    *username = '\0';

  if (scope)
    *scope = '\0';

  if (expires)
    *expires = 0;

  if (!server || !token)
  {
    if (server)
      snprintf(server->error, sizeof(server->error), "Bad arguments to function.");

    return (0);
  }

  if (!server->introspection_endpoint)
  {
    snprintf(server->error, sizeof(server->error), "Introspection not supported.");
    return (0);
  }

 /*
  * Prepare form data to get an access token...
  */

  num_form = cupsAddOption("token", token, num_form, &form);

  if ((form_data = _moauthFormEncode(num_form, form)) == NULL)
  {
    snprintf(server->error, sizeof(server->error), "Unable to encode form data.");
    goto done;
  }

  form_length = strlen(form_data);

 /*
  * Send a POST request with the form data...
  */

  if ((http = _moauthConnect(server->introspection_endpoint, resource, sizeof(resource))) == NULL)
  {
    snprintf(server->error, sizeof(server->error), "Connection to introspection endpoint failed: %s", cupsLastErrorString());
    goto done;
  }

  httpClearFields(http);
  httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/x-www-form-urlencoded");
  httpSetLength(http, form_length);

  if (httpPost(http, resource))
  {
    if (httpReconnect2(http, 30000, NULL))
    {
      snprintf(server->error, sizeof(server->error), "Reconnect failed: %s", cupsLastErrorString());
      goto done;
    }

    if (httpPost(http, resource))
    {
      snprintf(server->error, sizeof(server->error), "POST failed: %s", cupsLastErrorString());
      goto done;
    }
  }

  if (httpWrite2(http, form_data, form_length) < form_length)
  {
    snprintf(server->error, sizeof(server->error), "Write failed: %s", cupsLastErrorString());
    goto done;
  }

  while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

  if (status == HTTP_STATUS_OK)
  {
    json_data = _moauthCopyMessageBody(http);
    num_json  = _moauthJSONDecode(json_data, &json);

    if ((value = cupsGetOption("active", num_json, json)) != NULL)
      active = !strcmp(value, "true");

    if (username && (value = cupsGetOption("username", num_json, json)) != NULL)
    {
      strncpy(username, value, username_size - 1);
      username[username_size - 1] = '\0';
    }

    if (scope && (value = cupsGetOption("scope", num_json, json)) != NULL)
    {
      strncpy(scope, value, scope_size - 1);
      scope[scope_size - 1] = '\0';
    }

    if (expires && (value = cupsGetOption("exp", num_json, json)) != NULL)
      *expires = atoi(value);
  }
  else
  {
    snprintf(server->error, sizeof(server->error), "Unable to introspect access token: POST status %d", status);
  }

 /*
  * Return whatever we got...
  */

  done:

  httpClose(http);

  cupsFreeOptions(num_form, form);
  if (form_data)
    free(form_data);

  cupsFreeOptions(num_json, json);
  if (json_data)
    free(json_data);

  return (active);
}


/*
 * 'moauthPasswordToken()' - Get an access token using a username and password
 *                           (if supported by the OAuth server)
 */

char *					/* O - Access token or `NULL` on error */
moauthPasswordToken(
    moauth_t   *server,			/* I - Connection to OAuth server */
    const char *username,		/* I - Username string */
    const char *password,		/* I - Password string */
    const char *scope,			/* I - Scope to request or `NULL` */
    char       *token,			/* I - Access token buffer */
    size_t     tokensize,		/* I - Size of access token buffer */
    char       *refresh,		/* I - Refresh token buffer */
    size_t     refreshsize,		/* I - Size of refresh token buffer */
    time_t     *expires)		/* O - Expiration date/time, if known */
{
  http_t	*http = NULL;		/* HTTP connection */
  char		resource[256];		/* Token endpoint resource */
  http_status_t	status;			/* Response status */
  int		num_form = 0;		/* Number of form variables */
  cups_option_t	*form = NULL;		/* Form variables */
  char		*form_data = NULL;	/* POST form data */
  size_t	form_length;		/* Length of data */
  char		*json_data = NULL;	/* JSON response data */
  int		num_json = 0;		/* Number of JSON variables */
  cups_option_t	*json = NULL;		/* JSON variables */
  const char	*value;			/* JSON value */


 /*
  * Range check input...
  */

  if (token)
    *token = '\0';

  if (refresh)
    *refresh = '\0';

  if (expires)
    *expires = 0;

  if (!server || !username || !password || !token || tokensize < 32)
  {
    if (server)
      snprintf(server->error, sizeof(server->error), "Bad arguments to function.");

    return (NULL);
  }

  if (!server->token_endpoint)
  {
    snprintf(server->error, sizeof(server->error), "Authorization not supported.");
    return (0);
  }

 /*
  * Prepare form data to get an access token...
  */

  num_form = cupsAddOption("grant_type", "password", num_form, &form);
  num_form = cupsAddOption("username", username, num_form, &form);
  num_form = cupsAddOption("password", password, num_form, &form);
  if (scope)
    num_form = cupsAddOption("scope", scope, num_form, &form);

  if ((form_data = _moauthFormEncode(num_form, form)) == NULL)
  {
    snprintf(server->error, sizeof(server->error), "Unable to encode form data.");
    goto done;
  }

  form_length = strlen(form_data);

 /*
  * Send a POST request with the form data...
  */

  if ((http = _moauthConnect(server->token_endpoint, resource, sizeof(resource))) == NULL)
  {
    snprintf(server->error, sizeof(server->error), "Connection to token endpoint failed: %s", cupsLastErrorString());
    goto done;
  }

  httpClearFields(http);
  httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/x-www-form-urlencoded");
  httpSetLength(http, form_length);

  if (httpPost(http, resource))
  {
    if (httpReconnect2(http, 30000, NULL))
    {
      snprintf(server->error, sizeof(server->error), "Reconnect failed: %s", cupsLastErrorString());
      goto done;
    }

    if (httpPost(http, resource))
    {
      snprintf(server->error, sizeof(server->error), "POST failed: %s", cupsLastErrorString());
      goto done;
    }
  }

  if (httpWrite2(http, form_data, form_length) < form_length)
  {
    snprintf(server->error, sizeof(server->error), "Write failed: %s", cupsLastErrorString());
    goto done;
  }

  while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

  if (status == HTTP_STATUS_OK)
  {
    json_data = _moauthCopyMessageBody(http);
    num_json  = _moauthJSONDecode(json_data, &json);

    if ((value = cupsGetOption("access_token", num_json, json)) != NULL)
    {
      strncpy(token, value, tokensize - 1);
      token[tokensize - 1] = '\0';
    }

    if (expires && (value = cupsGetOption("expires_in", num_json, json)) != NULL)
      *expires = time(NULL) + atoi(value);

    if (refresh && (value = cupsGetOption("refresh_token", num_json, json)) != NULL)
    {
      strncpy(refresh, value, refreshsize - 1);
      refresh[refreshsize - 1] = '\0';
    }
  }
  else
  {
    snprintf(server->error, sizeof(server->error), "Unable to get access token - POST status %d", status);
  }

 /*
  * Return whatever we got...
  */

  done:

  httpClose(http);

  cupsFreeOptions(num_form, form);
  if (form_data)
    free(form_data);

  cupsFreeOptions(num_json, json);
  if (json_data)
    free(json_data);

  return (*token ? token : NULL);
}


/*
 * 'moauthRefreshToken()' - Refresh an access token from the OAuth server.
 */

char *					/* O - Access token or @code NULL@ on error */
moauthRefreshToken(
    moauth_t   *server,			/* I - Connection to OAuth server */
    const char *refresh,		/* I - Refresh token */
    char       *token,			/* I - Access token buffer */
    size_t     tokensize,		/* I - Size of access token buffer */
    char       *new_refresh,		/* I - Refresh token buffer */
    size_t     new_refreshsize,		/* I - Size of refresh token buffer */
    time_t     *expires)		/* O - Expiration date/time, if known */
{
  http_t	*http = NULL;		/* HTTP connection */
  char		resource[256];		/* Token endpoint resource */
  http_status_t	status;			/* Response status */
  int		num_form = 0;		/* Number of form variables */
  cups_option_t	*form = NULL;		/* Form variables */
  char		*form_data = NULL;	/* POST form data */
  size_t	form_length;		/* Length of data */
  char		*json_data = NULL;	/* JSON response data */
  int		num_json = 0;		/* Number of JSON variables */
  cups_option_t	*json = NULL;		/* JSON variables */
  const char	*value;			/* JSON value */


 /*
  * Range check input...
  */

  if (token)
    *token = '\0';

  if (new_refresh)
    *new_refresh = '\0';

  if (expires)
    *expires = 0;

  if (!server || !refresh || !token || tokensize < 32)
  {
    if (server)
      snprintf(server->error, sizeof(server->error), "Bad arguments to function.");

    return (NULL);
  }

  if (!server->token_endpoint)
  {
    snprintf(server->error, sizeof(server->error), "Authorization not supported.");
    return (0);
  }

 /*
  * Prepare form data to get an access token...
  */

  num_form = cupsAddOption("grant_type", "refresh_token", num_form, &form);
  num_form = cupsAddOption("refresh_token", refresh, num_form, &form);

  if ((form_data = _moauthFormEncode(num_form, form)) == NULL)
  {
    snprintf(server->error, sizeof(server->error), "Unable to encode form data.");
    goto done;
  }

  form_length = strlen(form_data);

 /*
  * Send a POST request with the form data...
  */

  if ((http = _moauthConnect(server->token_endpoint, resource, sizeof(resource))) == NULL)
  {
    snprintf(server->error, sizeof(server->error), "Connection to token endpoint failed: %s", cupsLastErrorString());
    goto done;
  }

  httpClearFields(http);
  httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/x-www-form-urlencoded");
  httpSetLength(http, form_length);

  if (httpPost(http, resource))
  {
    if (httpReconnect2(http, 30000, NULL))
    {
      snprintf(server->error, sizeof(server->error), "Reconnect to token endpoint failed: %s", cupsLastErrorString());
      goto done;
    }

    if (httpPost(http, resource))
    {
      snprintf(server->error, sizeof(server->error), "POST failed: %s", cupsLastErrorString());
      goto done;
    }
  }

  if (httpWrite2(http, form_data, form_length) < form_length)
  {
    snprintf(server->error, sizeof(server->error), "Write failed: %s", cupsLastErrorString());
    goto done;
  }

  while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

  if (status == HTTP_STATUS_OK)
  {
    json_data = _moauthCopyMessageBody(http);
    num_json  = _moauthJSONDecode(json_data, &json);

    if ((value = cupsGetOption("access_token", num_json, json)) != NULL)
    {
      strncpy(token, value, tokensize - 1);
      token[tokensize - 1] = '\0';
    }

    if (expires && (value = cupsGetOption("expires_in", num_json, json)) != NULL)
      *expires = time(NULL) + atoi(value);

    if (new_refresh && (value = cupsGetOption("refresh_token", num_json, json)) != NULL)
    {
      strncpy(new_refresh, value, new_refreshsize - 1);
      new_refresh[new_refreshsize - 1] = '\0';
    }
  }
  else
  {
    snprintf(server->error, sizeof(server->error), "Unable to get access token: POST status %d", status);
  }

 /*
  * Close the connection and return whatever we got...
  */

  done:

  httpClose(http);

  cupsFreeOptions(num_form, form);
  if (form_data)
    free(form_data);

  cupsFreeOptions(num_json, json);
  if (json_data)
    free(json_data);

  return (*token ? token : NULL);
}
