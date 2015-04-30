/* RealTime KML export of the payload data */
/* Based on https://ukhas.org.uk/using_google_earth */

#include <unistd.h>  	// UNIX standard function definitions
#include <stdio.h>   	// Standard input/output definitions
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "kml.h"
#include "global.h"

#define KML_LINE_SIZE 256 //Maximum size of a kml line

/* KML file footer */
const char footer_str[] = 
	"        </coordinates>\n" \
        "      </LineString>\n" \
       	"    </Placemark>\n" \
        "  </Document>\n" \
       	"</kml>\n";

/* Write the header of the KML file for the specified payload */
void writeHeader(FILE* fp, char * payload) {
	fprintf(fp,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(fp,"  <kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\" " );
	fprintf(fp,"  xmlns:kml=\"http://www.opengis.net/kml/2.2\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n");
	fprintf(fp,"  <Document>\n	<name>%s</name>\n", payload);
	fprintf(fp,"    <open>1</open>\n");
	fprintf(fp,"    <Placemark>\n");
	fprintf(fp,"      <name>%s-track</name>\n", payload);
	fprintf(fp,"      <LineString>\n");
	fprintf(fp,"        <extrude>1</extrude>\n");
	fprintf(fp,"        <tessellate>1</tessellate>\n");
	fprintf(fp,"        <altitudeMode>absolute</altitudeMode>\n");
	fprintf(fp,"        <coordinates>\n");

}

/* Write the footer of the KML file */
void writeFooter(FILE* fp) {
	fprintf(fp,"%s", footer_str);
}

/* Write the KML for refreshing the track of the specified payload */
void writeRefreshKML(FILE* fp, char * payload) {
	fprintf(fp,"<kml xmlns=\"http://earth.google.com/kml/2.0\">\n");
	fprintf(fp,"  <Document>\n");
	fprintf(fp,"    <NetworkLink>\n");
	fprintf(fp,"      <name>%s</name>\n", payload);
	fprintf(fp,"      <Url>\n");
	fprintf(fp,"        <href>%s.kml</href>\n", payload);
	fprintf(fp,"        <refreshMode>onInterval</refreshMode>\n");
	fprintf(fp,"        <refreshInterval>5</refreshInterval>\n");
	fprintf(fp,"      </Url>\n");
	fprintf(fp,"    </NetworkLink>\n");
	fprintf(fp,"  </Document>\n");
	fprintf(fp,"</kml>\n");
}

/* Update the KML file of the payload with the specified data */
void UpdatePayloadKML(char * payload, unsigned int seconds, double latitude, double longitude, unsigned int altitude) {
	FILE * fp;
	char filename[256];
	char line[KML_LINE_SIZE];
	struct stat st;
	int res;

	/* Discard empty positions */
	if (latitude == 0.0 && longitude == 0.0)
		return;	
	/* Path to the KML file for this payload */
	sprintf(filename, "%s.kml", payload);

	/* Check if the KML exists already */
	res = stat(filename, &st);

	/* Prepare the KML line for the payload data */
	snprintf(line, KML_LINE_SIZE, "          %f,%f,%d\n", 
			longitude, latitude, altitude);

	/* If the KML doesn't exists */
	if (res < 0) {
		/* Create a KML file which will refresh the payload KML at regular interval */
		char filename_refresh[256];
		FILE * fp_refresh;
		sprintf(filename_refresh, "%s-refresh.kml", payload);
		fp_refresh = fopen(filename_refresh, "w");
		if (fp_refresh) {
			writeRefreshKML(fp_refresh,payload);
			fclose(fp_refresh);
		} else {
			LogMessage("Can't open %s file for writing\n", filename_refresh);
		}
		/* Create the KML file for the payload */
		
		fp = fopen(filename, "a");
		if (!fp) {
			LogMessage("Can't open %s file for writing\n", filename);
			return;
		} else {
			LogMessage("Created %s file\n", filename);
		}
		/* Add the header, new data line and footer */
		writeHeader(fp, payload);
		fwrite(line, 1, strlen(line), fp);
		writeFooter(fp);

	} else {
		/* KML already exists : update only the file with new data */
		fp = fopen(filename, "r+");
		if (!fp) {
			LogMessage("Can't open %s file for writing\n", filename);
			return;
		}
		/* Write new data just before the position of the footer in the KML */
		fseek(fp, -strlen(footer_str), SEEK_END);
		fwrite(line, 1, strlen(line), fp);
		writeFooter(fp);
		
	}
	fclose(fp);
}


