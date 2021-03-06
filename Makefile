# Copyright (c) 2020 by Thomas A. Early N7TAE
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# locations for the executibles and other files are set here
# NOTE: IF YOU CHANGE THESE, YOU WILL NEED TO UPDATE THE service.* FILES AND
# if you change these locations, make sure the sgs.service file is updated!
# you will also break hard coded paths in the dashboard file, index.php.


# use this if you want debugging help in the case of a crash
#CPPFLAGS=-ggdb -W -std=c++11

# or, you can choose this for a much smaller executable without debugging help
CPPFLAGS=-W -std=c++11

LDFLAGS=-L/usr/lib -lrt

SRCS = $(wildcard *.cpp) $(wildcard $(IRC)/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

EXES = c2dec c2enc

all : $(EXES)

c2dec : c2dec.o codec2.o dump.o lpc.o nlp.o newamp1.o pack.o kiss_fft.o newamp2.o quantise.o codebooks.o newampbase.o qbase.o
	g++ -o $@ $^ $(LDFLAGS)

c2enc : c2enc.o codec2.o dump.o lpc.o nlp.o newamp1.o pack.o kiss_fft.o newamp2.o quantise.o codebooks.o newampbase.o qbase.o
	g++ -o $@ $^ $(LDFLAGS)

%.o : %.cpp
	g++ $(CPPFLAGS) -MMD -MD -c $< -o $@

.PHONY: clean

clean:
	$(RM) $(OBJS) $(DEPS) $(EXES)

-include $(DEPS)

test : $(EXES)
	aplay -f S16_LE test.raw
	./c2enc 3200 test.raw test.3200.dat && ./c2dec 3200 test.3200.dat - | aplay -f S16_LE
	./c2enc 2400 test.raw test.2400.dat && ./c2dec 2400 test.2400.dat - | aplay -f S16_LE
	./c2enc 1600 test.raw test.1600.dat && ./c2dec 1600 test.1600.dat - | aplay -f S16_LE
	./c2enc 1400 test.raw test.1400.dat && ./c2dec 1400 test.1400.dat - | aplay -f S16_LE
	./c2enc 1300 test.raw test.1300.dat && ./c2dec 1300 test.1300.dat - | aplay -f S16_LE
	./c2enc 1200 test.raw test.1200.dat && ./c2dec 1200 test.1200.dat - | aplay -f S16_LE
	./c2enc 700C test.raw test.0700.dat && ./c2dec 700C test.0700.dat - | aplay -f S16_LE
	./c2enc  450 test.raw test.0450.dat && ./c2dec  450 test.0450.dat - | aplay -f S16_LE
	./c2dec 450PWB test.0450.dat - | aplay -r 16000 -f S16_LE
	aplay -f S16_LE test.raw
	ls -l *.dat test.raw
