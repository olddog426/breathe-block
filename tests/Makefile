CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -g
INC = -I../BreatheBlock -Iarduino_stub
CORE = ../BreatheBlock/BreathField.cpp ../BreatheBlock/BreathScene.cpp

check: breath_field_test breath_scene_test stress_engine_test radar_phase_test
	./breath_field_test
	./breath_scene_test
	./stress_engine_test
	./radar_phase_test
	@echo "all tests passed"

breath_field_test: breath_field_test.cpp ../BreatheBlock/BreathField.cpp
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $^ -lm

breath_scene_test: breath_scene_test.cpp $(CORE)
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $^ -lm

stress_engine_test: stress_engine_test.cpp ../BreatheBlock/StressEngine.cpp
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $^ -lm

radar_phase_test: radar_phase_test.cpp ../BreatheBlock/LD6002.cpp
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $^ -lm

clean:
	rm -f breath_field_test breath_scene_test stress_engine_test radar_phase_test

.PHONY: check clean
