#include <iostream>
#include <vector>
#include <list>
#include <fstream>
#include <sstream>
#include <SFML/Graphics.hpp>

#define BASE_CRUISE_SPEED 166.0

#define EXAMPLE
//#define PORTUGAL
//#define TURKEY


class Aircraft;

#ifdef EXAMPLE
double legBase = 75.0; // [NM]
double legReservoir = 15.0; // [NM]
std::string suffix = "example";
#endif

#ifdef PORTUGAL
double legBase = 49.0; // [NM]
double legReservoir = 4.9; // [NM]
std::string suffix = "portugal2017";
#endif

#ifdef TURKEY
double legBase = 54.0; // [NM]
double legReservoir = 10.9; // [NM]
std::string suffix = "turkey2021";
#endif

double timeAtBase = 10.0; // [min]
double timeAtReservoir = 2.0; // [min]

double cruiseSpeed = BASE_CRUISE_SPEED / 60; // [NM / min]
double cruiseSpeedLegReservoir = 151.0 / 60; // [NM / min]

double cruisePower = 700; // [kW]

double turbinePower = (cruisePower + 200.0) / 60.0; // [kWh / min]
double turbineFuelConsumption = 0.27; // [kg / kWh]

double MTOW = 5670.0; // [kg]
double EW = 0.52 * MTOW; // [kg]
double waterCapacity = 0; // [kg]
double fuelCapacity = 0; // [kg]

double currentPower = 0;

double extinguishingAttack = 11000; // [kg]
int fleetSize = 1; // [-]
int baseCapacity = 1; // [-]
std::list<int> aircraftAtBase;


int t = 0; // [min]

int baseVisits = 0; // [kg]
double waterReleased = 0; // [kg]

std::vector<Aircraft> aircraft;


sf::RenderWindow* window = nullptr;


double CruisePower(double speed, double mass)
{
    double a = (mass * 9.81) / (6.578 * speed * speed);
    double power = (0.03 + 0.0485 * a * a) * (3.74 * speed * speed * speed) / 1000; // [kW]
    return power * 1.2;
}


enum class AircraftState
{
    ToBase,
    AtBase,
    ToReservoir,
    AtReservoir,
    ToFire,
    AtFire
};


class Aircraft
{
public:

    int id;

    AircraftState state;
    AircraftState lastState = AircraftState::AtBase;

    double timeToTransition; // [min]
    double distToTarget; // [NM]

    double fuel; // [kg]
    double water; // [kg]

    Aircraft()
        : id(0), state(AircraftState::AtBase), timeToTransition(0), distToTarget(0), fuel(0), water(0)
    { }


    Aircraft(const Aircraft& ac)
    {
        id = ac.id;
        state = ac.state;
        timeToTransition = ac.timeToTransition;
        distToTarget = ac.distToTarget;
        fuel = ac.fuel;
        water = ac.water;
    }

    void Tick()
    {
        double speed;
        double mass = EW + water + fuel;

        if ((state == AircraftState::ToReservoir && lastState == AircraftState::AtFire)
            || (state == AircraftState::ToFire && lastState == AircraftState::AtReservoir))
        {
            speed = cruiseSpeedLegReservoir;
        }
        else
        {
            speed = cruiseSpeed;
        }

        currentPower = CruisePower(speed * 60.0, mass);
        double fuelConsumption = (currentPower + 100.0) / 60.0 * turbineFuelConsumption;


        switch (state)
        {
            case AircraftState::ToBase:
                distToTarget -= speed;
                fuel -= fuelConsumption;
                if (distToTarget <= 0)
                {
                    ++baseVisits;
                    aircraftAtBase.emplace_back(id);
                    state = AircraftState::AtBase;
                    lastState = AircraftState::ToBase;
                    timeToTransition = timeAtBase;
                }
                break;

            case AircraftState::AtBase:
                if (timeToTransition <= 0)
                {
                    fuel = fuelCapacity;
                    state = AircraftState::ToReservoir;
                    lastState = AircraftState::AtBase;
                    distToTarget = legReservoir + legBase;
                }
                break;

            case AircraftState::ToReservoir:
                distToTarget -= speed;
                fuel -= fuelConsumption;
                if (distToTarget <= 0)
                {
                    state = AircraftState::AtReservoir;
                    lastState = AircraftState::ToReservoir;
                    timeToTransition = timeAtReservoir;
                }
                break;

            case AircraftState::AtReservoir:
                --timeToTransition;
                if (timeToTransition <= 0)
                {
                    water = waterCapacity;
                    state = AircraftState::ToFire;
                    lastState = AircraftState::AtReservoir;
                    distToTarget = legReservoir;
                }
                break;

            case AircraftState::ToFire:
                distToTarget -= speed;
                fuel -= fuelConsumption;
                if (distToTarget <= 0)
                {
                    lastState = AircraftState::ToFire;
                    state = AircraftState::AtFire;
                }
                break;

            case AircraftState::AtFire:
                if (fuel < 1.2 * turbinePower * turbineFuelConsumption / cruiseSpeed * legBase)
                {
                    waterReleased += water;
                    water = 0;
                    lastState = AircraftState::AtFire;
                    state = AircraftState::ToBase;
                    distToTarget = legBase;
                }
                break;
        }

        if (fuel < 0)
        {
            std::cerr << "FUEL EMPTY" << std::endl;
        }
    }

    void Extinguish ()
    {
        if (state == AircraftState::AtFire)
        {
            waterReleased += water;
            water = 0;

            lastState = AircraftState::AtFire;
            if (fuel < 1.2 * turbinePower * turbineFuelConsumption / cruiseSpeed * (2 * legReservoir + legBase))
            {
                state = AircraftState::ToBase;
                distToTarget = legBase;
            }
            else
            {
                state = AircraftState::ToReservoir;
                distToTarget = legReservoir;
            }
        }
    }

    double GetPosition() const
    {
        switch (state)
        {
            case AircraftState::ToBase:
                return distToTarget;

            case AircraftState::AtBase:
                return 0;

            case AircraftState::ToReservoir:
                return legReservoir + legBase - distToTarget;

            case AircraftState::AtReservoir:
                return legReservoir + legBase;

            case AircraftState::ToFire:
                return legBase + distToTarget;

            case AircraftState::AtFire:
                return legBase;
        }
    }
};


void UpdateGraphics()
{
    if (window == nullptr)
    {
        return;
    }

    if (t % 1 != 0)
    {
        return;
    }

    std::string title = "Mission Simulation (time = ";
    title = title.append(std::to_string(t));
    title = title.append("min)");
    window->setTitle(title);

    auto event = sf::Event();
    while (window->pollEvent(event))
    {
        if (event.type == sf::Event::Closed)
        {
            window->close();
            delete window;
            window = nullptr;
            return;
        }
    }

    window->clear(sf::Color(35, 35, 35));

    for (int i = 0; i < aircraft.size(); ++i)
    {
        sf::RectangleShape rect1;
        double a = (198 - 35) * aircraft[i].fuel / fuelCapacity;
        rect1.setFillColor(sf::Color(255 - (int)a, 35 + (int)a, 5));
        rect1.setSize(sf::Vector2f(40, 20));
        rect1.setPosition(sf::Vector2f(30 + 10 * aircraft[i].GetPosition(), 200 + i * 20));
        window->draw(rect1);

        if (aircraft[i].water > 0)
        {
            sf::RectangleShape rect2;
            rect2.setFillColor(sf::Color(35, 35, 198));
            rect2.setSize(sf::Vector2f(40, 5));
            rect2.setPosition(sf::Vector2f(30 + 10 * aircraft[i].GetPosition(), 215 + i * 20));
            window->draw(rect2);
        }
    }

    window->display();
    window->setFramerateLimit(30);
}


void SpawnAircraft()
{
    if ((aircraft.size() < fleetSize) && (t % 5 == 0))
    {
        aircraft.emplace_back();
        aircraft.back().id = aircraft.size() - 1;
        aircraftAtBase.emplace_back(aircraft.back().id);
    }
}


void BaseOperations()
{
    int i = 0;
    for (auto acID : aircraftAtBase)
    {
        if (i < baseCapacity)
        {
            aircraft[acID].timeToTransition -= 1;
        }
        ++i;
    }
    while (!aircraftAtBase.empty() && aircraft[aircraftAtBase.front()].timeToTransition <= 0)
    {
        aircraftAtBase.pop_front();
    }
}


void ExtinguishingOperations(bool force)
{
    if (force)
    {
        for (auto& ac : aircraft)
        {
            ac.Tick();
            if (ac.state == AircraftState::AtFire)
            {
                ac.Extinguish();
            }
        }
    }
    else
    {
        int aircraftAtFire = 0;
        for (auto& ac : aircraft)
        {
            ac.Tick();
            if (ac.state == AircraftState::AtFire)
            {
                ++aircraftAtFire;
            }
        }

        if (aircraftAtFire * waterCapacity > extinguishingAttack)
        {
            for (auto& ac : aircraft)
            {
                ac.Extinguish();
            }
        }
    }
}


void ResetSimulation()
{
    aircraft.resize(0);
    baseVisits = 0;
    waterReleased = 0;
}


int main()
{
    window = new sf::RenderWindow(sf::VideoMode(1000, 30 * 20 + 200), "Mission Simulation");
    std::stringstream filename;
    std::ofstream file;

    /***************************************************************
     Fuel Percentage Simulation
     ***************************************************************/

    filename.str("");
    filename << "fuel_percentage_simulation_" << suffix << ".dat";

    file.open(filename.str());
    file << "fuelPercentage" << ";"
         << "fuelCapacity" << ";"
         << "waterCapacity" << ";"
         << "baseVisits" << ";"
         << "waterReleased" << ";" << std::endl;

    for (int fuelPercentage = 10; fuelPercentage <= 90; ++fuelPercentage)
    {
        ResetSimulation();

        fuelCapacity = (MTOW - EW) * fuelPercentage / 100;
        waterCapacity = (MTOW - EW) - fuelCapacity;

        for (t = 0; t < 24 * 60; ++t)
        {
            SpawnAircraft();
            BaseOperations();
            ExtinguishingOperations(true);
            UpdateGraphics();
        }

        file << fuelPercentage << ";"
             << fuelCapacity << ";"
             << waterCapacity << ";"
             << baseVisits << ";"
             << waterReleased << ";" << std::endl;
    }

    file.close();


    /***************************************************************
     Fire-Reservoir-Speed Simulation
     ***************************************************************/

    filename.str("");
    filename << "fire_reservoir_speed_simulation_" << suffix << ".dat";

    file.open(filename.str());
    file << "speed" << ";"
         << "baseVisits" << ";"
         << "waterReleased" << ";" << std::endl;

    fuelCapacity = 700;
    waterCapacity = (MTOW - EW) - fuelCapacity;

    for (double v = 90; v <= 190; v += 0.1)
    {
        ResetSimulation();

        cruiseSpeedLegReservoir = v / 60.0;

        for (t = 0; t < 24 * 60; ++t)
        {
            SpawnAircraft();
            BaseOperations();
            ExtinguishingOperations(true);
            UpdateGraphics();
        }

        file << v << ";"
             << baseVisits << ";"
             << waterReleased << ";" << std::endl;
    }

    file.close();


    /***************************************************************
     Aircraft Time Simulation
     ***************************************************************/

    filename.str("");
    filename << "aircraft_time_simulation_" << suffix << ".dat";

    file.open(filename.str());
    file << "t" << ";"
         << "power" << ";"
         << "aircraftFuel" << ";"
         << "aircraftWater" << ";"
         << "waterReleased" << ";" << std::endl;

    ResetSimulation();

    fuelCapacity = 700;
    waterCapacity = (MTOW - EW) - fuelCapacity;

    cruiseSpeed = BASE_CRUISE_SPEED / 60.0;
    cruiseSpeedLegReservoir = BASE_CRUISE_SPEED / 60.0;

    for (t = 0; t < 24 * 60; ++t)
    {
        SpawnAircraft();
        BaseOperations();
        ExtinguishingOperations(true);
        UpdateGraphics();

        file << t << ";"
             << currentPower << ";"
             << aircraft.begin()->fuel << ";"
             << aircraft.begin()->water << ";"
             << waterReleased << ";" << std::endl;
    }

    file.close();


    /***************************************************************
     Fleet Simulation
     ***************************************************************/

    std::vector<int> baseCapacityIterations {1, 2, 5, 10};

    for (auto bc : baseCapacityIterations)
    {
        baseCapacity = bc;

        filename.str("");
        filename << "fleet_simulation_" << suffix << "_base_capacity_" << baseCapacity << ".dat";

        file.open(filename.str());
        file << "fleetSize" << ";"
             << "baseVisits" << ";"
             << "waterReleased" << ";" << std::endl;

        fuelCapacity = 700;
        waterCapacity = (MTOW - EW) - fuelCapacity;

        for (fleetSize = 1; fleetSize < 200; ++fleetSize)
        {
            ResetSimulation();

            for (t = 0; t < 24 * 60; ++t)
            {
                SpawnAircraft();
                BaseOperations();
                ExtinguishingOperations(false);
                UpdateGraphics();
            }

            file << fleetSize << ";"
                 << baseVisits << ";"
                 << waterReleased << ";" << std::endl;
        }

        file.close();
    }


    if (window)
    {
        window->close();
        delete window;
    }

    return 0;
}
