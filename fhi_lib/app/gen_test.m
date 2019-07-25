%/******************************************************************************
%*
%*   Copyright (c) 2019 Intel.
%*
%*   Licensed under the Apache License, Version 2.0 (the "License");
%*   you may not use this file except in compliance with the License.
%*   You may obtain a copy of the License at
%*
%*       http://www.apache.org/licenses/LICENSE-2.0
%*
%*   Unless required by applicable law or agreed to in writing, software
%*   distributed under the License is distributed on an "AS IS" BASIS,
%*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
%*   See the License for the specific language governing permissions and
%*   limitations under the License.
%*
%*******************************************************************************/

%Matlab: read bin
%fileID_c = fopen('ant_7.bin','r');
%ant7_c= fread(fileID_c, [2, 792*14*80], 'integer*2');
%ant7_c;
%ant7_c=ant7_c.';

close all;
clear all;

ifft_in = load('ifft_in.txt')
ant_c = ifft_in;
for (i=1:1:80*14-1)
    ant_c = [ant_c; ifft_in];
end

ant0=ant_c;
ant1=ant_c*10;
ant2=ant_c*20;
ant3=ant_c*30;
ant4=ant_c*40;
ant5=ant_c*50;
ant6=ant_c*60;
ant7=ant_c*70;
ant8=ant_c*80;
ant9=ant_c*90;
ant10=ant_c*100;
ant11=ant_c*110;
ant12=ant_c*120;
ant13=ant_c*130;
ant14=ant_c*140;
ant15=ant_c*150;


ant0_16=int16(ant0.');
fileID = fopen('ant_0.bin','w');
fwrite(fileID,ant0_16, 'int16');
fclose(fileID);

ant1_16=int16(ant1.');
fileID = fopen('ant_1.bin','w');
fwrite(fileID,ant1_16, 'int16');
fclose(fileID);

ant2_16=int16(ant2.');
fileID = fopen('ant_2.bin','w');
fwrite(fileID,ant2_16, 'int16');
fclose(fileID);

ant3_16=int16(ant3.');
fileID = fopen('ant_3.bin','w');
fwrite(fileID,ant3_16, 'int16');
fclose(fileID);

ant4_16=int16(ant4.');
fileID = fopen('ant_4.bin','w');
fwrite(fileID,ant4_16, 'int16');
fclose(fileID);

ant5_16=int16(ant5.');
fileID = fopen('ant_5.bin','w');
fwrite(fileID,ant5_16, 'int16');
fclose(fileID);

ant6_16=int16(ant6.');
fileID = fopen('ant_6.bin','w');
fwrite(fileID,ant6_16, 'int16');
fclose(fileID);

ant7_16=int16(ant7.');
fileID = fopen('ant_7.bin','w');
fwrite(fileID,ant7_16, 'int16');
fclose(fileID);

ant8_16=int16(ant8.');
fileID = fopen('ant_8.bin','w');
fwrite(fileID,ant8_16, 'int16');
fclose(fileID);

ant9_16=int16(ant9.');
fileID = fopen('ant_9.bin','w');
fwrite(fileID,ant9_16, 'int16');
fclose(fileID);

ant10_16=int16(ant10.');
fileID = fopen('ant_10.bin','w');
fwrite(fileID,ant10_16, 'int16');
fclose(fileID);

ant11_16=int16(ant11.');
fileID = fopen('ant_11.bin','w');
fwrite(fileID,ant11_16, 'int16');
fclose(fileID);

ant12_16=int16(ant12.');
fileID = fopen('ant_12.bin','w');
fwrite(fileID,ant12_16, 'int16');
fclose(fileID);

ant13_16=int16(ant13.');
fileID = fopen('ant_13.bin','w');
fwrite(fileID,ant13_16, 'int16');
fclose(fileID);

ant14_16=int16(ant14.');
fileID = fopen('ant_14.bin','w');
fwrite(fileID,ant14_16, 'int16');
fclose(fileID);

ant15_16=int16(ant15.');
fileID = fopen('ant_15.bin','w');
fwrite(fileID,ant15_16, 'int16');
fclose(fileID);
