typedef enum {stNone, stString, stInteger, stDouble, stBoolean} TSettingType;

struct TSetting
{
	int SectionIndex;
	int Index;
	char ValueName[32];
	TSettingType SettingType;
	char *StringValuePtr;
	int *IntValuePtr;
	double *DoubleValuePtr;
	int MaxValueLength;
	// void (Callback)(int);
};

void RegisterConfigFile(char *Filename);
int RegisterConfigSection(char *Section);
int RegisterConfigString(int SectionIndex, int Index, char *Name, char *StringValuePtr, int MaxValueLength, void (Callback)(int));
int RegisterConfigInteger(int SectionIndex, int Index, char *Name, int *IntValuePtr, void (Callback)(int));
int RegisterConfigDouble(int SectionIndex, int Index, char *Name, double *DoubleValuePtr, void (Callback)(int));
int RegisterConfigBoolean(int SectionIndex, int Index, char *Name, int *BoolValuePtr, void (Callback)(int));
int ReadConfigValue(int SettingIndex);
void SetConfigValue(char *Setting, char *Value);
int SettingAsString(int SettingIndex, char *SettingName, int SettingNameSize, char *SettingValue, int SettingValueSize);
void SaveConfigFile(void);


