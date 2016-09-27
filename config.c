//// Generalised configuration manager
// Generalised configuration manager

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

#define MAX_SECTIONS	16
#define SECTION_LENGTH	21
#define MAX_SETTINGS	256

char *ConfigFilename;

int SectionCount=0;
char Sections[MAX_SECTIONS][32];

int SettingsCount=0;
struct TSetting Settings[MAX_SETTINGS];

void RegisterConfigFile(char *Filename)
{
	ConfigFilename = Filename;
}

int RegisterConfigSection(char *Section)
{
	int i;
	
	for (i=0; i<SectionCount; i++)
	{
		if (strcmp(Section, Sections[i]) == 0)
		{
			return i;
		}
	}
	
	if (SectionCount < MAX_SECTIONS)
	{
		strcpy(Sections[SectionCount], Section);
		return SectionCount++;
	}
	
	return -1;
}

int FindSettingIndex(int SectionIndex, int Index, char *Name)
{
	// Return existing position for this setting, if it's there, or allocate a new one
	int i;
	
	for (i=0; i<SettingsCount; i++)
	{
		if ((Settings[i].SectionIndex == SectionIndex) &&
			(Settings[i].Index == Index) &&
			(strcmp(Settings[i].ValueName, Name) == 0))
		{
			// Found
			return i;
		}
	}
	
	// Room for one more on top ?
	if (SettingsCount < MAX_SETTINGS)
	{
		Settings[SettingsCount].SectionIndex = SectionIndex;
		Settings[SettingsCount].Index = Index;
		strcpy(Settings[SettingsCount].ValueName, Name);
		
		return SettingsCount++;
	}
	
	// Full or can't add, and not found, so return "fail"
	return -1;
}

int RegisterConfigDouble(int SectionIndex, int Index, char *Name, double *DoubleValuePtr, void (Callback)(int))
{
	if ((SectionIndex >= 0) && (SectionIndex < SectionCount))
	{
		int SettingIndex;
				
		if ((SettingIndex = FindSettingIndex(SectionIndex, Index, Name)) >= 0)
		{
			Settings[SettingIndex].SectionIndex = SectionIndex;
			Settings[SettingIndex].Index = Index;
			strcpy(Settings[SettingIndex].ValueName, Name);
			Settings[SettingIndex].SettingType = stDouble;
			Settings[SettingIndex].DoubleValuePtr = DoubleValuePtr;
			// Settings[SettingIndex].Callback = Callback;
			
			ReadConfigValue(SettingIndex);
		}
	}
	
	return 0;
}

int RegisterConfigInteger(int SectionIndex, int Index, char *Name, int *IntValuePtr, void (Callback)(int))
{
	if ((SectionIndex >= 0) && (SectionIndex < SectionCount))
	{
		int SettingIndex;
				
		if ((SettingIndex = FindSettingIndex(SectionIndex, Index, Name)) >= 0)
		{
			Settings[SettingIndex].SectionIndex = SectionIndex;
			Settings[SettingIndex].Index = Index;
			strcpy(Settings[SettingIndex].ValueName, Name);
			Settings[SettingIndex].SettingType = stInteger;
			Settings[SettingIndex].IntValuePtr = IntValuePtr;
			// Settings[SettingIndex].Callback = Callback;
			
			return ReadConfigValue(SettingIndex);
		}
	}
	
	return 0;
}

int RegisterConfigBoolean(int SectionIndex, int Index, char *Name, int *BoolValuePtr, void (Callback)(int))
{
	if ((SectionIndex >= 0) && (SectionIndex < SectionCount))
	{
		int SettingIndex;
				
		if ((SettingIndex = FindSettingIndex(SectionIndex, Index, Name)) >= 0)
		{
			Settings[SettingIndex].SectionIndex = SectionIndex;
			Settings[SettingIndex].Index = Index;
			strcpy(Settings[SettingIndex].ValueName, Name);
			Settings[SettingIndex].SettingType = stBoolean;
			Settings[SettingIndex].IntValuePtr = BoolValuePtr;
			// Settings[SettingIndex].Callback = Callback;
			
			return ReadConfigValue(SettingIndex);
		}
	}
	
	return 0;
}

int RegisterConfigString(int SectionIndex, int Index, char *Name, char *StringValuePtr, int MaxValueLength, void (Callback)(int))
{
	if ((SectionIndex >= 0) && (SectionIndex < SectionCount))
	{
		int SettingIndex;
				
		if ((SettingIndex = FindSettingIndex(SectionIndex, Index, Name)) >= 0)
		{
			Settings[SettingIndex].SectionIndex = SectionIndex;
			Settings[SettingIndex].Index = Index;
			strcpy(Settings[SettingIndex].ValueName, Name);
			Settings[SettingIndex].SettingType = stString;
			Settings[SettingIndex].StringValuePtr = StringValuePtr;
			Settings[SettingIndex].MaxValueLength = MaxValueLength;
			// Settings[SettingIndex].Callback = Callback;
			
			return ReadConfigValue(SettingIndex);
		}
	}
	
	return 0;
}

void GetLongName(int SettingIndex, char *ValueName, int Length)
{
	struct TSetting *Setting;
	
	Setting = &(Settings[SettingIndex]);
	
	// Build string to look for in file
	if (Setting->Index >= 0)
	{
		sprintf(ValueName, "%s%s_%d", Sections[Setting->SectionIndex], Setting->ValueName, Setting->Index);
	}
	else
	{
		sprintf(ValueName, "%s%s", Sections[Setting->SectionIndex], Setting->ValueName);
	}
}

int ReadConfigValue(int SettingIndex)
{
    char line[100], ValueName[64], *token, *temp;
	struct TSetting *Setting;
	FILE *fp;
	
	GetLongName(SettingIndex, ValueName, sizeof(ValueName));

	Setting = &(Settings[SettingIndex]);	

    if ((fp = fopen(ConfigFilename, "r" ) ) != NULL)
    {
		while (fgets(line, sizeof(line), fp) != NULL)
		{
			line[strcspn(line, "\r")] = '\0';		// Get rid of CR if there is one
		
			token = strtok(line, "=" );
			if (strcasecmp(ValueName, token ) == 0)
			{
				temp = strtok( NULL, "\n");
				
				switch (Setting->SettingType)
				{
					case stString:
						strncpy(Setting->StringValuePtr, temp, Setting->MaxValueLength-1);
					break;
					
					case stInteger:
						*(Setting->IntValuePtr) = atoi(temp);
					break;
					
					case stDouble:
						// So we can handle bandwidths such as "20K8", convert "K" to decimal point
						if (strchr(temp, 'K') != NULL)
						{
							*strchr(temp, 'K') = '.';
						}
						*(Setting->DoubleValuePtr) = atof(temp);
					break;
					
					case stBoolean:
						*(Setting->IntValuePtr) = (*temp == '1') || (*temp == 'Y') || (*temp == 'y') || (*temp == 't') || (*temp == 'T');
					break;
					
					case stNone:
					break;
				}
				fclose(fp);
				return 1;
			}
		}
		fclose(fp);
    }
	
	return 0;
}

int FindSettingByName(char *ValueNameToFind)
{
	char ValueName[64];
	int SettingIndex;
		
	for (SettingIndex=0; SettingIndex<SettingsCount; SettingIndex++)
	{
		GetLongName(SettingIndex, ValueName, sizeof(ValueName));
		
		if (strcasecmp(ValueNameToFind, ValueName) == 0)
		{
			// Found
			return SettingIndex;
		}
	}
	return -1;
}


void SaveConfigFile(void)
{
    char line[100], original[100], *token;
	FILE *src, *dest;
	struct TSetting *Setting;
	char *TempFileName="gateway.txt.tmp";
	char *SavedFileName="gateway.txt.old";
	int SettingIndex;

    if ((src = fopen(ConfigFilename, "r" ) ) != NULL)
    {
		if ((dest = fopen(TempFileName, "w" ) ) != NULL)
		{
			while (fgets(line, sizeof(line), src) != NULL)
			{
				line[strcspn(line, "\r")] = '\0';		// Get rid of CR if there is one
				strcpy(original, line);
			
				token = strtok(line, "=" );				// Name of setting
				
				if (token == NULL)
				{
					// Write old line
					fputs(original, dest);
				}
				else if ((SettingIndex = FindSettingByName(token)) >= 0)
				{
					// Found, so write new value instead of old one
					// tracker=M0RPI/5
					Setting = &(Settings[SettingIndex]);
					switch (Setting->SettingType)
					{
						case stString:
							fprintf(dest, "%s=%s\n", token, Setting->StringValuePtr);
						break;
						
						case stInteger:
							fprintf(dest, "%s=%d\n", token, *(Setting->IntValuePtr));
						break;
						
						case stDouble:
							fprintf(dest, "%s=%lf\n", token, *(Setting->DoubleValuePtr));
						break;
						
						case stBoolean:
							fprintf(dest, "%s=%c\n", token, *(Setting->IntValuePtr) ? 'Y' : 'N');
						break;
						
						case stNone:
						break;
					}
				}
				else
				{
					// Just write old line
					fputs(original, dest);
				}
			}
			fclose(dest);
			
			// Now save original file and replace with new one
			remove(SavedFileName);
			rename(ConfigFilename, SavedFileName);
			rename(TempFileName, ConfigFilename);
		}
		fclose(src);
    }
}

void SetConfigValue(char *Setting, char *Value)
{
	int SettingIndex;

	if ((SettingIndex = FindSettingByName(Setting)) >= 0)
	{
		switch (Settings[SettingIndex].SettingType)
		{
			case stString:
				strncpy(Settings[SettingIndex].StringValuePtr, Value, Settings[SettingIndex].MaxValueLength-1);
			break;
			
			case stInteger:
				*Settings[SettingIndex].IntValuePtr = atoi(Value);
			break;
			
			case stDouble:
				*Settings[SettingIndex].DoubleValuePtr = atof(Value);
			break;
			
			case stBoolean:
				*Settings[SettingIndex].IntValuePtr = Value[strcspn(Value, "1YyTt")];
			break;
			
			case stNone:
			break;
		}
	}
}


int SettingAsString(int SettingIndex, char *SettingName, int SettingNameSize, char *SettingValue, int SettingValueSize)
{
	if ((SettingIndex >= 0) && (SettingIndex < SettingsCount))
	{
		// strncpy(SettingName, Settings[Index].ValueName, SettingNameSize);
	
		GetLongName(SettingIndex, SettingName, sizeof(SettingNameSize));
		
		switch (Settings[SettingIndex].SettingType)
		{
			case stString:
				snprintf(SettingValue, SettingValueSize-1, "\"%s\"", Settings[SettingIndex].StringValuePtr);
			break;
			
			case stInteger:
				snprintf(SettingValue, SettingValueSize-1, "%d", *Settings[SettingIndex].IntValuePtr);
			break;
			
			case stDouble:
				snprintf(SettingValue, SettingValueSize-1, "%lf", *Settings[SettingIndex].DoubleValuePtr);
			break;
			
			case stBoolean:
				snprintf(SettingValue, SettingValueSize-1, "%d", *Settings[SettingIndex].IntValuePtr ? 1 : 0);
			break;
			
			case stNone:
				strncpy(SettingValue, "\"?\"", SettingValueSize);
			break;
		}
	
		return 1;
	}
	
	return 0;
}