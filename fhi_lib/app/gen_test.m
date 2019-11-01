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

close all;
clear all;

%select mu and bw to generate test files
sub6=false;  %false
mu=3; % 0,1, or 3
bw=100; %5,10,20,100 MHz

nSlots=160; % any 40 and 160

     %  5MHz    10MHz   15MHz   20 MHz  25 MHz  30 MHz  40 MHz  50MHz   60 MHz  70 MHz  80 MHz   90 MHz  100 MHz
nNumRbsPerSymF1 = ...
[
     %  5MHz    10MHz   15MHz   20 MHz  25 MHz  30 MHz  40 MHz  50MHz   60 MHz  70 MHz  80 MHz   90 MHz  100 MHz
        [25,    52,     79,     106,    133,    160,    216,    270,    0,         0,      0,      0,      0]         % Numerology 0 (15KHz)
        [11,    24,     38,     51,     65,     78,     106,    133,    162,       0,    217,    245,    273]         % Numerology 1 (30KHz)
        [0,     11,     18,     24,     31,     38,     51,     65,     79,        0,    107,    121,    135]         % Numerology 2 (60KHz)
];

nNumRbsPerSymF2 = ...
[
    %  50Mhz  100MHz  200MHz   400MHz
        [66,    132,    264,     0]        % Numerology 2 (60KHz)
        [32,    66,     132,     264]      % Numerology 3 (120KHz)
];

if sub6
    disp('Sub6')
    if mu < 3
        nNumerology = mu+1;
        switch (bw)
            case {5}
                numRBs = nNumRbsPerSymF1(nNumerology,0+1);
            case {10}
                numRBs = nNumRbsPerSymF1(nNumerology,1+1);
            case {15}
                numRBs = nNumRbsPerSymF1(nNumerology,2+1);
            case {20}
                numRBs = nNumRbsPerSymF1(nNumerology,3+1);
            case {25}
                numRBs = nNumRbsPerSymF1(nNumerology,4+1);
            case {30}
                numRBs = nNumRbsPerSymF1(nNumerology,5+1);
            case {40}
                numRBs = nNumRbsPerSymF1(nNumerology,6+1);
            case {50}
                numRBs = nNumRbsPerSymF1(nNumerology,7+1);
            case {60}
                numRBs = nNumRbsPerSymF1(nNumerology,8+1);
            case {70}
                numRBs = nNumRbsPerSymF1(nNumerology,9+1);
            case {80}
                numRBs = nNumRbsPerSymF1(nNumerology,10+1);
            case {90}
                numRBs = nNumRbsPerSymF1(nNumerology,11+1);
            case {100}
                numRBs = nNumRbsPerSymF1(nNumerology,12+1);
            otherwise
                disp('Unknown BW && mu')
        end
    end
else
    disp('mmWave')
    if  (mu >=2) && (mu <= 3)
        nNumerology = mu;
        switch (bw)
            case {50}
                numRBs = nNumRbsPerSymF2(nNumerology-1,0+1);
            case {100}
                numRBs = nNumRbsPerSymF2(nNumerology-1,1+1);
            case {200}
                numRBs = nNumRbsPerSymF2(nNumerology-1,2+1);
            case {400}
                numRBs = nNumRbsPerSymF2(nNumerology-1,3+1);
            otherwise
                disp('Unknown BW && mu')
        end
    end
end

if numRBs ==0
    disp('Incorrect Numerology and BW combination.')
    return
end

bw
numRBs
nSlots

%use file as input
%ifft_in = load('ifft_in.txt')

%gen IQs
ifft_in = [[1:1:(numRBs*12)]', [1:1:(numRBs*12)]'];

ant_c = ifft_in;
for (i=1:1:nSlots*14-1)
    ifft_in_1 = ifft_in + i;
    ant_c = [ant_c; ifft_in_1];
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
