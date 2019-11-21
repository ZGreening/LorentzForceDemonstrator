
#include <ctime>
#include <stdlib.h>
#include <cmath>
#include <wiringPi.h>
#include <iostream>
#include <memory>
#include <string>
#include <grpc++/grpc++.h>

#ifdef BAZEL_BUILD
#include "/Microcontroller/grpcMessageSerialization.grpc.pb.h"
#else
#include "grpcMessageSerialization.grpc.pb.h"
#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpcserver::messagePassing;
using grpcserver::setVariables;
using grpcserver::variablesSet;

/**
 * uses <http://wiringpi.com/> for GPIO pin standard
 * This is for local invocation of RPI controlling methods.
 * RPC implementation for client-server interactions currently under development
 */

class Microcontroller
{
public:
	Microcontroller(const unsigned int gearRatio, const unsigned int potentiometerDegrees);

	void step(int voltage, int motor);

	// The motor sequence for the full step stepping method.
	static const bool FULL_STEP_MOTOR_SEQUENCE[][4];

	/** Holds the Raspberry Pi pin numbers for pins A though D of the stepper motor. **/
	int inputPins[16];

	//Holds the maximum steps the motor should turn based on
	//gear ratio of pulleys and degrees of potentiometer turn radius
	double maxSteps;

	//Holds the size of steps of accelerating potentiometer for 1v change
	double accelStepSize;

	//Holds the size of steps of deflecting potentiometer for 1v change
	double deflectStepSize;

	//Holds the size of steps of magnetizing current potentiometer for .01a change.
	double magStepSize;

	void writeSequence(const unsigned int sequenceNo, int motor);

	//activates or deactivates the relay for switches.
	//passes [0,0,0,0] for each of the 4 switches as low
	void writeRelay(int relay[4]);
};
//Holds the current position of accelerating potentiometer
static int accelValue = 0;

//Holds the current position of deflecting potentiometer
static int deflectValue = 50;

//Holds the current position of magnetizing potentiometer
static int magValue = 0;

static int relay[4] = {1, 1, 1, 1};

void assignValues(const setVariables &passed)
{
	Microcontroller controller1(2, 270);
	controller1.step(passed.deflectingv(), 2);
	controller1.step(passed.accelv(), 1);
	controller1.step(passed.current(), 3);
	int magSwitch = passed.magneticarc();
	int deflectSwitch = passed.deflectingpolarity();

	/*std::cout << "Deflecting Voltage = " << passed.deflectingv()
		<< "\nAccelerating Voltage = " << passed.accelv()
		<< "\nMagnetizing Current = " << passed.current()
		<< "\nMagnetic Arc = " << passed.magneticArc()
		<< "\nDeflecting Polarity = " << passed.deflectingpolarity()
		<< std::endl << std::endl;*/

	switch (magSwitch)
	{
	case 0: //off
		relay[0] = 1;
		relay[1] = 1;
		break;
	case 1: //clockwise
		relay[0] = 0;
		relay[1] = 1;
		break;
	case 2: //counterclockwise
		relay[0] = 1;
		relay[1] = 0;
		break;
	}
	switch (deflectSwitch)
	{
	case 0: //off
		relay[2] = 1;
		relay[3] = 1;
		break;
	case 1: //positive
		relay[2] = 0;
		relay[3] = 0;
		break;
	case 2: //negative
		relay[2] = 1;
		relay[3] = 1;
		break;
	}
	controller1.writeRelay(relay);
}
// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public messagePassing::Service
{
	Status sendVariables(ServerContext *context, const setVariables *request,
						 variablesSet *reply) override
	{
		//std::cout<< request->relay4()<<std::endl;
		assignValues(*request);
		return Status::OK;
	}
};
const bool Microcontroller::FULL_STEP_MOTOR_SEQUENCE[][4] = {
	{1, 1, 0, 0},
	{0, 1, 1, 0},
	{0, 0, 1, 1},
	{1, 0, 0, 1},
	{1, 1, 0, 0},
	{0, 1, 1, 0},
	{0, 0, 1, 1},
	{1, 0, 0, 1}};

Microcontroller::Microcontroller(const unsigned int gearRatio, const unsigned int potentiometerDegrees)
{
	inputPins[0] = 0; //accel1
	inputPins[1] = 2;
	inputPins[2] = 3;
	inputPins[3] = 1; //accel4
	inputPins[4] = 8; //deflect1
	inputPins[5] = 9;
	inputPins[6] = 7;
	inputPins[7] = 15; //deflect4
	inputPins[8] = 21; //mag1
	inputPins[9] = 22;
	inputPins[10] = 23;
	inputPins[11] = 24; //mag4
	inputPins[12] = 6;  //relay1
	inputPins[13] = 10;
	inputPins[14] = 11;
	inputPins[15] = 31; //relay4
	for (int i = 0; i < 16; i++)
	{
		pinMode(inputPins[i], OUTPUT);
		if (i < 12)
			digitalWrite(inputPins[i], 0);
		else
			digitalWrite(inputPins[i], 1);
	}

	maxSteps = ((2036.8 * potentiometerDegrees) / 360) * gearRatio;

	accelStepSize = maxSteps / 250;
	deflectStepSize = maxSteps / 200;
	magStepSize = maxSteps / 300;
}

void Microcontroller::step(int voltage, int motor)
{
	switch (motor)
	{
	case 1: //Accelerating Voltage motor
		if ((voltage > accelValue) && (voltage <= 250))
		{

			for (int step = std::round((voltage - accelValue) * accelStepSize); step > 0; step--)
			{
				int currentSequenceNo = step % 8;
				writeSequence(currentSequenceNo, 1);
			}
			accelValue = voltage;
		}
		else if (voltage <= 250)
		{
			for (int step = 0; step < std::abs(std::round((voltage - accelValue) * accelStepSize)); step++)
			{
				int currentSequenceNo = step % 8;
				writeSequence(currentSequenceNo, 1);
			}
			accelValue = voltage;
		}
		break;
	case 2: //Deflecting voltage motor
		if ((voltage > deflectValue) && (voltage <= 250))
		{

			for (int step = std::round((voltage - deflectValue) * deflectStepSize); step > 0; step--)
			{
				int currentSequenceNo = step % 8;
				writeSequence(currentSequenceNo, 2);
			}
			deflectValue = voltage;
		}
		else if (voltage <= 250)
		{
			for (int step = 0; step < std::abs(std::round((voltage - deflectValue) * deflectStepSize)); step++)
			{
				int currentSequenceNo = step % 8;
				writeSequence(currentSequenceNo, 2);
			}
			deflectValue = voltage;
		}
		break;
	case 3: //Magnetizing Current Amperage
		if ((voltage > magValue) && (voltage <= 300))
		{

			for (int step = std::round((voltage - magValue) * magStepSize); step > 0; step--)
			{
				int currentSequenceNo = step % 8;
				writeSequence(currentSequenceNo, 3);
			}
			magValue = voltage;
		}
		else if (voltage <= 300)
		{
			for (int step = 0; step < std::abs(std::round((voltage - magValue) * magStepSize)); step++)
			{
				int currentSequenceNo = step % 8;
				writeSequence(currentSequenceNo, 3);
			}
			magValue = voltage;
		}
		break;
	}
}

void Microcontroller::writeSequence(const unsigned int sequenceNo, int motor)
{

	for (int i = 0; i < 4; i++)
	{
		switch (motor)
		{
		case 1:
			digitalWrite(inputPins[i], FULL_STEP_MOTOR_SEQUENCE[sequenceNo][i]);
			break;
		case 2:
			digitalWrite(inputPins[i + 4], FULL_STEP_MOTOR_SEQUENCE[sequenceNo][i]);
			break;
		case 3:
			digitalWrite(inputPins[i + 8], FULL_STEP_MOTOR_SEQUENCE[sequenceNo][i]);
			break;
		}
	}
	delay(3); //this is the speed of the motor.
			  //lower wait is faster iteration through steps
	for (int i = 0; i < 4; i++)
	{
		switch (motor)
		{
		case 1:
			digitalWrite(inputPins[i], 0);
			break;
		case 2:
			digitalWrite(inputPins[i + 4], 0);
			break;
		case 3:
			digitalWrite(inputPins[i + 8], 0);
			break;
		}
	}
}

void Microcontroller::writeRelay(int relay[4])
{
	for (int i = 0; i < 4; i++)
		digitalWrite(inputPins[i + 12], relay[i]);

	//std::cout<< "hi im relay!" << relay[3] << std::endl;
}

void RunServer()
{
	std::string server_address("0.0.0.0:50051");
	GreeterServiceImpl service;
	//	Microcontroller controller(2, 270);

	ServerBuilder builder;
	// Listen on the given address without any authentication mechanism.
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	// Register "service" as the instance through which we'll communicate with
	// clients. In this case it corresponds to an *synchronous* service.
	builder.RegisterService(&service);
	// Finally assemble the server.
	std::unique_ptr<Server> server(builder.BuildAndStart());
	std::cout << "Server listening on " << server_address << std::endl;

	// Wait for the server to shutdown. Note that some other thread must be
	// responsible for shutting down the server for this call to ever return.
	server->Wait();
}

int main(int argc, char **argv)
{
	if (wiringPiSetup() == -1)
	{
		std::cerr << "'Failed to setup wiringPi" << std::endl;
		exit(EXIT_FAILURE);
	}
	RunServer();

	return 0;
}
