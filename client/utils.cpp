#include "client.h"

int IndexOf(std::string str, char c)
{
	int i = 0;
	while (i < str.length()){
		if (str[i] == c)
			return (i);
		i++;
	}
	return (-1);
}

char	*ft_substr(std::string s, int start, int len)
{
	char	*res;
	int		i;

	i = 0;	
	res = (char *)malloc(sizeof(char) * (len + 1));
	if (res == NULL)
		return (NULL);
	while (i < (int)len && s[start])
		res[i++] = s[start++];
	res[i] = '\0';
	return (res);
}