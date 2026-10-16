/*
		Project:		Moon Lander
		Module:			stadtSimu.cpp
		Description:	A City Simulation
		Author:			Martin G‰ckler
		Address:		Hofmannsthalweg 14, A-4030 Linz
		Web:			https://www.gaeckler.at/

		Copyright:		(c) 2026 Martin G‰ckler

		This program is free software: you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation, version 3.

		You should have received a copy of the GNU General Public License
		along with this program. If not, see <http://www.gnu.org/licenses/>.

		THIS SOFTWARE IS PROVIDED BY Martin G‰ckler, Linz, Austria ``AS IS''
		AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
		TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
		PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
		CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
		SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
		LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
		USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
		ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
		OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
		OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
		SUCH DAMAGE.
*/

// --------------------------------------------------------------------- //
// ----- switches ------------------------------------------------------ //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- includes ------------------------------------------------------ //
// --------------------------------------------------------------------- //

#include <gak/math.h>
#include <gak/array.h>
#include <gak/queue.h>
#include <gak/iostream.h>

#include <winlib/ControlWindow.h>
#include <winlib/WINAPP.H>
#include <winlib/POPUP.H>
#include <winlib/DEVICE.H>
#include <winlib/colors.h>

#include "stadtSimu_rc.h"

// --------------------------------------------------------------------- //
// ----- imported datas ------------------------------------------------ //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- module switches ----------------------------------------------- //
// --------------------------------------------------------------------- //

#ifdef __BORLANDC__
#	pragma option -RT-
#	pragma option -b
#	pragma option -a4
#	pragma option -pc
#endif

using namespace winlib;

// --------------------------------------------------------------------- //
// ----- constants ----------------------------------------------------- //
// --------------------------------------------------------------------- //

/*
	here are the constant for the city emulation engine
*/
const long		START_MONEY = 10000000;				// cent

const gak::uint64	WORKER_TRIGGER = 10;			// the employee minimum count
const gak::uint64	LARGE_WORKER_TRIGGER = 1000;	// small city trigger
const double	WORKER_FACTOR1 = 0.5;				// the employee part of the population for small cities
const double	WORKER_FACTOR2 = 0.3;				// the employee part of the population for large sities

// decision for constructing new residential buildings
const double	MAX_FREE_BEDS_FACTOR = 0.2;			// the max part of free beds before constructing new buildings
const double	MAX_FREE_BEDS_FACTOR_LARGE = 0.1;	// ... for large buildings

// decision for constructing new commercial/industrial buildings
const double	MAX_FREE_JOBS_FACTOR = 0.2;			// the max part of free jobs before constructing new buildings
const double	MAX_FREE_JOBS_FACTOR_LARGE = 0.1;	// ... for large buildings
const size_t	MINIMUM_JOB_COUNT = 100;			// the minimum number of jobs, if there are fewer jobs we do not check the job factors

// decision for moving into the city
const double	POPULATION_CHANGE_BY_BEDS = 0.1;	// moving in factor by free beds
const double	POPULATION_CHANGE_BY_JOBS = 0.1;	// moving in factor by free jobs
const int		SLEEP_TRIGGER = 20;					// The first Beds less than this values are occupied earlier

const int		WATER_RANDOM = 3;	// the probability for Water-dots
const int		TREE_RANDOM = 3;	// ... for trees

// decision for new working contracts
const double	JOB_CHANGE = 0.05;

// city costs
const gak::int64	MAYOR_INCOME = 1500;
const gak::int64	TOWN_HALL_COST = 1000;
const gak::int64	FIRE_COST = 2000;
const gak::int64	POLICE_COST = 2000;
const gak::int64	SCHOOL_COST = 2000;
const gak::int64	HOSPITAL_COST = 2000;
const gak::int64	HOMELESS_COST = 1000;
const gak::int64	STREET_COST = 500;
const gak::int64	TRAMWAY_COST = 500;

const gak::int64	UNEMPLOY_LOSS = 500;

// city income
const gak::int64	WORKER_TAX = 500;

// maximum distance to transport lines before construction any buildings
const int MAX_TRANSPORT_DISTANCE=3;

const int		AREA_PIXEL_SIZE=16;
const int		CITY_SIZE=128;
const int		MAX_ARERA_ID = CITY_SIZE*CITY_SIZE;

static const char * const createTypes[] = {
	"Gew‰sser",
	"Land",
	"Straﬂe",
	"Trambahn",
	"Wohngebiet",
	"Handel",
	"Industrie",

	"Rathaus",
	"Schule",
	"Krankenhaus",
	"Feuerwehr",
	"Polizei",
	"Kraftwerk",
	"Wasserwerk",
};

enum CreateType {
	ctWaterArea,
	ctLand,
	ctStreet,
	ctTramWay,
	ctLiving,
	ctCommerce,
	ctIndustry,

	ctTownHall,
	ctSchool,
	ctHospital,
	ctFireBrigade,
	ctPolice,
	ctEnergy,
	ctWaterPlant,
};

const char FILE_NAME[]="theCity.city";
const gak::uint32 magic = 0x3826;
const gak::uint16 version = 0x1;

// --------------------------------------------------------------------- //
// ----- macros -------------------------------------------------------- //
// --------------------------------------------------------------------- //

template <typename ID_T>
inline bool checkValidID( ID_T index )
{
	return (index >= 0 && index < MAX_ARERA_ID);
}

#define assertValidID( index )	assert( checkValidID( index ) )

// --------------------------------------------------------------------- //
// ----- type definitions ---------------------------------------------- //
// --------------------------------------------------------------------- //

struct Border
{
	int top:1;
	int right:1;
	int bottom:1;
	int left:1;
	int count:4;
};

struct AreaInfo
{
	const char	*name;
	gak::int64	price;

	AreaInfo(const char *name=nullptr, gak::int64 price=0) 
		: name(name), price(price)
	{}
};

struct BuildingInfo
{
	const char	*name;
	gak::int64	price;
	size_t		beds,
				jobs,
				supportCount;
	long		powerReq,
				waterReq;
	Icon		*icon;
	BuildingInfo(const char *name=nullptr, gak::int64 price=0, size_t beds=0, size_t jobs=0, long powerReq=0, long waterReq=0, Icon *icon=nullptr, size_t supportCount=0) 
		: name(name), price(price),
		beds(beds), jobs(jobs), 
		powerReq(powerReq), waterReq(waterReq), 
		icon(icon),
		supportCount(supportCount)
	{}
};

struct CityArea
{
	enum AreaType
	{
		atUNSPEC,
		atGREEN, atWATER, atSTREET, atTRAMWAY, atCROSSING, atLIVING, atCOMMERCIAL, atINDUSTRY, atPUBLIC_BUILDING
	} areaType;
	enum BuildingType
	{
		btUNSPEC,
		
		btPOLICE, btFIRE, btHOSPITAL, btTOWN_HALL, btSCHOOL, btENERGY, btWATER_PLANT,

		pbtSMALL_LIVING, pbtLARGE_LIVING,
		pbtSMALL_COMMERCIAL, pbtLARGE_COMMERCIAL,
		pbtSMALL_INDUSTRY, pbtLARGE_INDUSTRY

	} buildingType;
	static const int dependent = 0;
	static const int master = 1;
	static const int tree = 2;
	gak::Bitfield	bits;
	gak::int32	width, height;

	void makeMaster( int width=2, int height=2 )
	{
		bits.set( dependent ).set( master );
		this->width = width;
		this->height = height;
	}
	void makeDependent()
	{
		bits.set( dependent );
	}
	void fromBinaryStream( std::istream &stream );
	void toBinaryStream( std::ostream &stream ) const;
};


namespace gak
{
	template<>
	inline int compare( const CreateType &elem1, const CreateType &elem2 )
	{
		return scalarCompare( elem1, elem2 );
	}

	template<>
	inline int compare( const CityArea::BuildingType &elem1, const CityArea::BuildingType &elem2 )
	{
		return scalarCompare( elem1, elem2 );
	}

	template<>
	inline int compare( const CityArea::AreaType &elem1, const CityArea::AreaType &elem2 )
	{
		return scalarCompare( elem1, elem2 );
	}
}

// --------------------------------------------------------------------- //
// ----- class definitions --------------------------------------------- //
// --------------------------------------------------------------------- //

class CityWindow : public OverlappedWindow
{
	typedef long												AreaID;
	typedef gak::PODarray<AreaID>								AreaIDs;
	typedef gak::PairMap<CityArea::AreaType, AreaIDs>			AreaMapList;
	typedef gak::PairMap<CityArea::BuildingType, gak::uint64>	BuildingCountMap;
	
	gak::Queue<AreaID>	m_areaQueue;

	struct TheCity
	{
		gak::PODarray<CityArea>	areas;
		AreaMapList				areaMapList;
		BuildingCountMap		buildingCount;

		gak::int64				money;
		gak::uint64				population,
								beds,
								jobs,
								occupiedJobs;
		gak::int32				powerValue, 
								waterValue;

		gak::Queue<AreaID>		&m_areaQueue;
		/*
			beds and population
		*/
		long getBedGap() const
		{
			return long(beds - population);
		}
		double getFreeBedFactor() const
		{
			return beds ? double(getBedGap())/double(beds) : 0;
		}
		gak::uint64 getHomeLess() const
		{
			return population > beds ? population - beds : 0;
		}

		/*
			jobs ans workers
		*/
		long getFreeJobs() const
		{
			return long(jobs - occupiedJobs);
		}
		double getFreeJobFactor() const
		{
			return jobs ? double(getFreeJobs())/double(jobs) : 0;
		}
		double getOcupiedJobFactor() const
		{
			return jobs ? double(occupiedJobs)/double(jobs) : 0;
		}
		size_t getWorkers() const
		{
			static gak::math::ProgressTriggerFactor workerFactors[] =
			{
				{WORKER_TRIGGER, 1},
				{LARGE_WORKER_TRIGGER, WORKER_FACTOR1},
				{0, WORKER_FACTOR2}
			};
			return gak::math::round<size_t>(gak::math::progressiveFactor(double(population), workerFactors));
		}
		long getUnemployed() const
		{
			return long(getWorkers() - occupiedJobs);
		}
		double getUnemployedFactor() const
		{
			size_t workers = getWorkers();
			if( workers )
			{
				return double(getUnemployed())/double(workers);
			}
			return 0;
		}

		/*
			Areas
		*/
		size_t getAreaCount( CityArea::AreaType areaType ) const
		{
			return areaMapList.hasElement(areaType) ? areaMapList[areaType].size() : 0;
		}

		/*
			I/O
		*/
		void rebuild()
		{
			// the list of areas with private buildings
			static const CityArea::AreaType buildingAreas[] = {CityArea::atLIVING, CityArea::atCOMMERCIAL, CityArea::atINDUSTRY};
			CityArea *area = areas.getDataHandle();
			AreaID id = 0;

			for( ; id < areas.size(); ++id, ++area )
			{
				CityArea::AreaType areaType = area->areaType;
				if( CityArea::atUNSPEC != areaType )
				{
					areaMapList[areaType].addElement( id );

					CityArea::BuildingType buildingType = area->buildingType;
					if( CityArea::btUNSPEC != buildingType )
					{
						++buildingCount[buildingType];
					}
					else 
					{
						FOR_EACH( i, buildingAreas, 
						{
							if( areaType == buildingAreas[i] && !area->dependent )
							{
								m_areaQueue.push( id );
								break;
							}
						});
					}

				}
			}
		}
		void fromBinaryStream( std::istream &stream )
		{
			gak::fromBinaryStream(stream, &areas);
			gak::fromBinaryStream(stream, &money);
			gak::fromBinaryStream(stream, &population);
			gak::fromBinaryStream(stream, &occupiedJobs);

			rebuild();

			gak::fromBinaryStream(stream, &beds);
			gak::fromBinaryStream(stream, &jobs);
			gak::fromBinaryStream(stream, &powerValue);
			gak::fromBinaryStream(stream, &waterValue);
		}
		void toBinaryStream( std::ostream &stream ) const
		{
			gak::toBinaryStream(stream, areas);
			gak::toBinaryStream(stream, money);
			gak::toBinaryStream(stream, population);
			gak::toBinaryStream(stream, occupiedJobs);

			gak::toBinaryStream(stream, beds);
			gak::toBinaryStream(stream, jobs);
			gak::toBinaryStream(stream, powerValue);
			gak::toBinaryStream(stream, waterValue);
		}

		TheCity(gak::Queue<AreaID >&areaQueue) :
			areas( MAX_ARERA_ID ), 
			money( START_MONEY ), population(1), jobs(1), occupiedJobs(1), beds(0),
			powerValue(0), waterValue(0), m_areaQueue(areaQueue)
		{
		}
	}	m_theCity;

	ToolWindow				m_tools;
	ComboBox				m_creatSelector;

	Label					m_moneyLabel, m_moneyCount;
	Label					m_populationLabel, m_populationCount;
	Label					m_bedLabel, m_bedCount;
	Label					m_jobLabel, m_jobCount;
	Label					m_occupiedJobsLabel, m_occupiedJobsCount;
	Label					m_powerLabel, m_powerCount;
	Label					m_waterLabel, m_waterCount;
	Label					m_separator;
	Label					m_workerLabel, m_workerCount;
	Label					m_successLabel, m_successCount;

	AreaID					m_lastToolHelp;

	static int lineNo( AreaID index )
	{
		assertValidID( index );
		return index/CITY_SIZE;
	}
	static int colNo( AreaID index )
	{
		assertValidID( index );
		return index%CITY_SIZE;
	}
	static AreaID leftID( AreaID index );
	static AreaID topID( AreaID index );
	static AreaID rightID( AreaID index );
	static AreaID bottomID( AreaID index );
	static AreaID idForPoint( const Point &point );

	static int getDistance( int col1, int line1, int col2, int line2 );
	static int getDistance( AreaID index1, AreaID index2 );

	AreaID findGoodTransport(CityArea::AreaType transportType, AreaID index) const;
	AreaID findGoodTransport(AreaID index) const
	{
		AreaID transportID = findGoodTransport( CityArea::atTRAMWAY, index );
		if( transportID < 0 )
			transportID = findGoodTransport( CityArea::atSTREET, index );
		return transportID;
	}
	void selectRandomArea( CityArea::AreaType type );
	bool processArea(AreaID index);
	gak::int64 getCost();
	void nextCityStep();
	void removeHouse( CityArea *area );
	gak::int64 constructHouse2( CityArea *area, AreaID index, CityArea::BuildingType building );
	gak::int64 constructHouse( CityArea *area, AreaID index, CityArea::BuildingType building )
	{
		gak::int64 price = 0;
		if( area->buildingType != building )
			price = constructHouse2( area, index, building );
		return price;
	}
	void showToolHelp(AreaID index);
	void updateStatus();
	Border calcBorder( AreaID index );
	void drawShape( Device &dev, int x, int y, const Border &border );
	void drawArea( Device &dev, AreaID index, int x, int y );
	CityArea *checkLargeArea( AreaID *index ) const;
	void createLargeArea( CityArea *master, AreaID index );
	void changeAreaType( AreaID index );

	ProcessStatus handleCreate() override;
	SuccessCode handleClose() override;
	ProcessStatus handleMouseMove( ModifierFlags modifier, const Point &position ) override;
	ProcessStatus handleLeftButton( LeftButton leftButton, ModifierFlags modifier, const Point &position ) override;
	ProcessStatus handleRepaint( Device &hDC ) override;
	void handleTimer() override;

	/*
		success
	*/
	double getSuccessFactor() const;

	public:
	CityWindow();
};

class StadtSimulationApp : public Application
{
	bool startApplication( HINSTANCE /*hInstance*/, const char * /*cmdLine*/ ) override;
	CallbackWindow *createMainWindow( const char * /*cmdLine*/, int /*nCmdShow*/ ) override
	{
		std::unique_ptr<CityWindow>	mainWindow( new CityWindow );
		if( mainWindow->create( nullptr ) == scERROR )
		{
			throw gak::LibraryException( "Could not create window!" );
		}

		mainWindow->focus();
		return mainWindow.release();
	}
	void deleteMainWindow( BasicWindow  *mainWindow ) override
	{
		delete mainWindow;
	}

	public:
	StadtSimulationApp() : Application( IDI_STADT_SIMU ) {}
};

// --------------------------------------------------------------------- //
// ----- exported datas ------------------------------------------------ //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- module static data -------------------------------------------- //
// --------------------------------------------------------------------- //

static Icon	s_police,
			s_fire,
			s_hospital,
			s_townHall,
			s_schoolS,
			s_schoolL,
			s_livingS,
			s_livingL,
			s_commercialS,
			s_commercialL,
			s_industryS,
			s_industryL,
			s_railCrossing,
			s_voltaic,
			s_waterPlant;

static StadtSimulationApp	app;


// static Building Information per BuildingType
typedef gak::PairMap<CityArea::BuildingType, BuildingInfo>	BuildingInfoMap;
typedef BuildingInfoMap::value_type							BuildingInfoPair;
static const BuildingInfoPair s_buildingInfoList[] =
{	//																  Name							Price	Beds/Jobs	Power/Water	Icon			For People 
	//																													Consumption					Count
	BuildingInfoPair( CityArea::pbtSMALL_LIVING,		BuildingInfo( "Kleines Wohngeb‰ude",		-1,		10, 0,		10,	10,		&s_livingS				) ),
	BuildingInfoPair( CityArea::pbtLARGE_LIVING,		BuildingInfo( "Groﬂes Wohngeb‰ude",			-1,		100, 0,		100, 100,	&s_livingL				) ),
	BuildingInfoPair( CityArea::pbtSMALL_COMMERCIAL,	BuildingInfo( "Kleines Gesch‰ftsgeb‰ude",	-1,		0, 10,		10, 10,		&s_commercialS			) ),
	BuildingInfoPair( CityArea::pbtLARGE_COMMERCIAL,	BuildingInfo( "Groﬂes Gesch‰ftsgeb‰ude",	-1,		0, 100,		100, 100,	&s_commercialL			) ),
	BuildingInfoPair( CityArea::pbtSMALL_INDUSTRY,		BuildingInfo( "Gewerbehaus",				-1,		0, 10,		20, 10,		&s_industryS			) ),
	BuildingInfoPair( CityArea::pbtLARGE_INDUSTRY,		BuildingInfo( "Fabrik",						-1,		0, 200,		400, 100,	&s_industryL			) ),
	BuildingInfoPair( CityArea::btFIRE,					BuildingInfo( "Feuerwehr",					100000,	0, 20,		10, 100,	&s_fire,		5000	) ),
	BuildingInfoPair( CityArea::btPOLICE,				BuildingInfo( "Polizei",					100000,	0, 20,		20, 20,		&s_police,		5000	) ),
	BuildingInfoPair( CityArea::btHOSPITAL,				BuildingInfo( "ƒrzte",						100000,	0, 20,		20, 20,		&s_hospital,	5000	) ),
	BuildingInfoPair( CityArea::btSCHOOL,				BuildingInfo( "Bildung",					50000,	0, 10,		20, 20,		nullptr,		5000	) ),
	BuildingInfoPair( CityArea::btTOWN_HALL,			BuildingInfo( "Rathaus",					50000,	0, 10,		20, 20,		&s_townHall,	5000	) ),
	BuildingInfoPair( CityArea::btENERGY,				BuildingInfo( "Kraftwerk",					300000, 0, 5,		-2000, 0,	&s_voltaic ) ),
	BuildingInfoPair( CityArea::btWATER_PLANT,			BuildingInfo( "Wasserwerk",					200000, 0, 5,		0,-2000,	&s_waterPlant ) ),
};
static const BuildingInfoMap s_buildingInfos(s_buildingInfoList);


// static Area Information per AreaType
typedef gak::PairMap<CityArea::AreaType, AreaInfo>	AreaInfoMap;
typedef AreaInfoMap::value_type						AreaInfoPair;
static const AreaInfoPair s_areaInfoList[] =
{
	AreaInfoPair( CityArea::atLIVING, AreaInfo( "Wohngebiet", 10000 ) ),
	AreaInfoPair( CityArea::atCOMMERCIAL, AreaInfo( "Gewerbegebiet", 10000 ) ),
	AreaInfoPair( CityArea::atINDUSTRY, AreaInfo( "Industiegebiet", 10000 ) ),
	AreaInfoPair( CityArea::atPUBLIC_BUILDING, AreaInfo( "÷ffentliches Geb‰ude", 10000 ) ),
	AreaInfoPair( CityArea::atWATER, AreaInfo( "Gew‰sser", 0 ) ),
	AreaInfoPair( CityArea::atGREEN, AreaInfo( "Wald/Wiese", 0 ) ),
	AreaInfoPair( CityArea::atSTREET, AreaInfo( "Straﬂe", 50000 ) ),
	AreaInfoPair( CityArea::atTRAMWAY, AreaInfo( "Tram-/S-bahn", 50000 ) ),
	AreaInfoPair( CityArea::atCROSSING, AreaInfo( "Bahn¸bergang", 70000 ) )
};
static const AreaInfoMap s_areaInfos(s_areaInfoList);


// Map CreateType -> AreaType
typedef gak::PairMap<CreateType, CityArea::AreaType>	CreateAreaMap;
typedef CreateAreaMap::value_type						CreateAreaPair;
static const CreateAreaPair	s_buildingAreaList[] =
{
	CreateAreaPair(ctWaterArea, CityArea::atWATER),
	CreateAreaPair(ctLand, CityArea::atGREEN),
	CreateAreaPair(ctStreet, CityArea::atSTREET),
	CreateAreaPair(ctTramWay, CityArea::atTRAMWAY),
	CreateAreaPair(ctLiving, CityArea::atLIVING),
	CreateAreaPair(ctCommerce, CityArea::atCOMMERCIAL),
	CreateAreaPair(ctIndustry, CityArea::atINDUSTRY),
	CreateAreaPair(ctTownHall, CityArea::atPUBLIC_BUILDING),
	CreateAreaPair(ctFireBrigade, CityArea::atPUBLIC_BUILDING),
	CreateAreaPair(ctPolice, CityArea::atPUBLIC_BUILDING),
	CreateAreaPair(ctSchool, CityArea::atPUBLIC_BUILDING),
	CreateAreaPair(ctHospital, CityArea::atPUBLIC_BUILDING),
	CreateAreaPair(ctEnergy, CityArea::atPUBLIC_BUILDING),
	CreateAreaPair(ctWaterPlant, CityArea::atPUBLIC_BUILDING),
};
static const CreateAreaMap s_createAreaMap(s_buildingAreaList);


// Map CreateType -> BuildingType
typedef gak::PairMap<CreateType, CityArea::BuildingType>	CreateBuildingMap;
typedef CreateBuildingMap::value_type						CreateBuildingPair;
static const CreateBuildingPair	s_buildingMapList[] =
{
	CreateBuildingPair(ctTownHall, CityArea::btTOWN_HALL),
	CreateBuildingPair(ctFireBrigade, CityArea::btFIRE),
	CreateBuildingPair(ctPolice, CityArea::btPOLICE),
	CreateBuildingPair(ctSchool, CityArea::btSCHOOL),
	CreateBuildingPair(ctEnergy, CityArea::btENERGY),
	CreateBuildingPair(ctWaterPlant, CityArea::btWATER_PLANT),
	CreateBuildingPair(ctHospital, CityArea::btHOSPITAL),
};
static const CreateBuildingMap s_createBuildingMap(s_buildingMapList);

// --------------------------------------------------------------------- //
// ----- class static data --------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- prototypes ---------------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- module functions ---------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- class inlines ------------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- class constructors/destructors -------------------------------- //
// --------------------------------------------------------------------- //

CityWindow::CityWindow() : OverlappedWindow( nullptr ), 
	m_tools(nullptr), m_creatSelector(nullptr), m_lastToolHelp(-1), m_theCity( m_areaQueue )
{
	setText("Stadtsimulation");
}

// --------------------------------------------------------------------- //
// ----- class static functions ---------------------------------------- //
// --------------------------------------------------------------------- //


void CityArea::fromBinaryStream( std::istream &stream )
{
	int tmpType;
	gak::fromBinaryStream( stream, &tmpType );
	areaType = AreaType(tmpType);
	gak::fromBinaryStream( stream, &tmpType );
	buildingType = BuildingType(tmpType);
	gak::fromBinaryStream( stream, &bits );
	gak::fromBinaryStream( stream, &width );
	gak::fromBinaryStream( stream, &height );
}

void CityArea::toBinaryStream( std::ostream &stream ) const
{
	int tmpType = areaType;
	gak::toBinaryStream( stream, tmpType );
	tmpType = buildingType;
	gak::toBinaryStream( stream, tmpType );
	gak::toBinaryStream( stream, bits );
	gak::toBinaryStream( stream, width );
	gak::toBinaryStream( stream, height );
}

CityWindow::AreaID CityWindow::leftID( AreaID index )
{
	assertValidID( index );

	if( colNo( index ) )
		return index -1;
	else
		return -1;
}

CityWindow::AreaID CityWindow::topID( AreaID index )
{
	assertValidID( index );
	return index - CITY_SIZE;
}

CityWindow::AreaID CityWindow::rightID( AreaID index )
{
	assertValidID( index );

	index += 1;
	if( checkValidID(index) && colNo( index ) )
		return index;
	else
		return -1;
}

CityWindow::AreaID CityWindow::bottomID( AreaID index )
{
	assertValidID( index );

	index += CITY_SIZE;
	return index < MAX_ARERA_ID ? index : -1;
}


CityWindow::AreaID CityWindow::idForPoint( const Point &point )
{
	int x = point.x / AREA_PIXEL_SIZE;
	if( x < 0 || x>=CITY_SIZE )
		return -1;

	int y = point.y / AREA_PIXEL_SIZE;
	if( y < 0 || y>=CITY_SIZE )
		return -1;

	return y*CITY_SIZE+x;
}

int CityWindow::getDistance( int col1, int line1, int col2, int line2 )
{
	int colDist = gak::math::abs( col1-col2 );
	int lineDist = gak::math::abs( line1-line2 );
	return ((colDist+lineDist)*3)>>2;
}

int CityWindow::getDistance( AreaID index1, AreaID index2 )
{
	int col1 = colNo(index1 );
	int col2 = colNo(index2 );
	int line1 = lineNo(index1 );
	int line2 = lineNo(index2 );
	return getDistance( col1, line1, col2, line2 );
}

// --------------------------------------------------------------------- //
// ----- class privates ------------------------------------------------ //
// --------------------------------------------------------------------- //

double CityWindow::getSuccessFactor() const
{
	gak::math::Mean<double>	successFactor;

	CityArea::BuildingType	typesToCheck[] =
	{
		CityArea::btFIRE, CityArea::btPOLICE, CityArea::btHOSPITAL, CityArea::btSCHOOL, CityArea::btTOWN_HALL
	};
	const double population = m_theCity.population ? double(m_theCity.population) : 1;
	FOR_EACH( i, typesToCheck, {
		const CityArea::BuildingType buildingType = typesToCheck[i];
		const BuildingInfo &info = s_buildingInfos[buildingType];
		if( m_theCity.buildingCount.hasElement(buildingType) )
		{
			const size_t count = m_theCity.buildingCount[buildingType];
			successFactor.add( gak::math::min(double(count * info.supportCount) / population,1.0) );
		}
	});

	const std::size_t	greenCount = m_theCity.getAreaCount(CityArea::atGREEN);
	const std::size_t	waterCount = m_theCity.getAreaCount(CityArea::atWATER);
	const std::size_t	goodCount = greenCount + waterCount;

	const std::size_t	livingCount = m_theCity.getAreaCount(CityArea::atLIVING);
	const std::size_t	commercialCount = m_theCity.getAreaCount(CityArea::atCOMMERCIAL);
	const std::size_t	industryCount = m_theCity.getAreaCount(CityArea::atINDUSTRY);
	const std::size_t	badCount = livingCount + commercialCount + industryCount;

	successFactor.add( badCount ? gak::math::min(double(goodCount) / badCount,1.0) : 0);

	return successFactor.getMean();
}

CityWindow::AreaID CityWindow::findGoodTransport(CityArea::AreaType transportType, AreaID index) const
{
	if( m_theCity.areaMapList.hasElement( transportType ) )
	{
		const AreaIDs &areaIDs = m_theCity.areaMapList[transportType];
		int col = colNo( index );
		int line = lineNo( index );
		for( AreaIDs::const_iterator it = areaIDs.cbegin(), endIT = areaIDs.cend(); it != endIT; ++it )
		{
			int index2 = *it;
			int col2 = colNo( index2 );
			int line2 = lineNo( index2 );
			int dist = getDistance( col, line, col2, line2 );
			if( dist < MAX_TRANSPORT_DISTANCE )
				return index2;
		}
	}
	return -1;
}

void CityWindow::selectRandomArea( CityArea::AreaType type )
{
	if( m_theCity.areaMapList.hasElement( type ) )
	{
		const AreaIDs &list = m_theCity.areaMapList[type];
		const size_t size = list.size();
		if( size )
		{
			const size_t sel = size_t(randomNumber(int(list.size())));
			m_areaQueue.push( int(list[sel]) );
		}
	}
}

bool CityWindow::processArea(AreaID index)
{
	CityArea	&area = m_theCity.areas[index];
	AreaID		masterIndex = index;
	CityArea	*master = checkLargeArea( &masterIndex );

	CityArea::BuildingType	newType = CityArea::btUNSPEC;

	if( area.areaType == CityArea::atLIVING )
	{
		double freeBeds = m_theCity.getFreeBedFactor();
		if( freeBeds < MAX_FREE_BEDS_FACTOR )
		{
			if( master && freeBeds < MAX_FREE_BEDS_FACTOR_LARGE )
			{
				newType = CityArea::pbtLARGE_LIVING;
			}
			else
			{
				master = nullptr;
				newType = CityArea::pbtSMALL_LIVING;
			}
		}
	}
	else if( area.areaType == CityArea::atCOMMERCIAL )
	{
		double freeJobs = (m_theCity.jobs < MINIMUM_JOB_COUNT) ? 0 : m_theCity.getFreeJobFactor();
		if( freeJobs < MAX_FREE_JOBS_FACTOR )
		{
			if( master && freeJobs < MAX_FREE_JOBS_FACTOR_LARGE )
			{
				newType = CityArea::pbtLARGE_COMMERCIAL;
			}
			else
			{
				master = nullptr;
				newType = CityArea::pbtSMALL_COMMERCIAL;
			}
		}
	}
	else if( area.areaType == CityArea::atINDUSTRY )
	{
		double freeJobs = m_theCity.getFreeJobFactor();
		if( freeJobs < MAX_FREE_JOBS_FACTOR )
		{
			if( master && freeJobs < MAX_FREE_JOBS_FACTOR_LARGE )
			{
				newType = CityArea::pbtLARGE_INDUSTRY;
			}
			else
			{
				master = nullptr;
				newType = CityArea::pbtSMALL_INDUSTRY;
			}
		}
	}

	if( CityArea::btUNSPEC != newType )
	{
		if( master )
		{
			if( constructHouse( master, masterIndex, newType ) )
				createLargeArea( master, masterIndex );
		}
		else if( !area.bits.test(CityArea::dependent) )
			constructHouse( &area, index, newType );

		updateStatus();
	}
	return CityArea::btUNSPEC != area.buildingType || area.dependent;
}

gak::int64 CityWindow::getCost()
{
	static struct
	{
		CityArea::BuildingType	type;
		gak::int64				cost;
	} 
	s_publicBuildingCost[] =
	{
		{CityArea::btTOWN_HALL,	TOWN_HALL_COST},
		{CityArea::btFIRE,		FIRE_COST},
		{CityArea::btPOLICE,	POLICE_COST},
		{CityArea::btSCHOOL,	SCHOOL_COST},
		{CityArea::btHOSPITAL,	HOSPITAL_COST},
	};

	gak::int64	cost = MAYOR_INCOME;
	const double ocupiedJobsFactor = m_theCity.getOcupiedJobFactor();
	FOR_EACH( i, s_publicBuildingCost, {
		const CityArea::BuildingType	type = s_publicBuildingCost[i].type;
		const gak::uint64				count = m_theCity.buildingCount[type];

		cost += count * s_publicBuildingCost[i].cost;
		cost += gak::math::round<gak::int64>(s_buildingInfos[type].jobs * count * WORKER_TAX * 2 * ocupiedJobsFactor);
	});

	cost += m_theCity.areaMapList[CityArea::atSTREET].size() * STREET_COST;
	cost += m_theCity.areaMapList[CityArea::atTRAMWAY].size() * TRAMWAY_COST;

	cost += m_theCity.getHomeLess() * HOMELESS_COST;

	return cost;
}

void CityWindow::nextCityStep()
{
	using gak::math::DoubleScale;
	static const DoubleScale	s_scale( DoubleScale::Pair(0,1), DoubleScale::Pair(-1,1) );
	const long bedGap = m_theCity.getBedGap();
	const long absBedGap = gak::math::abs(bedGap);
	const long absBedsPopulationChange = absBedGap < SLEEP_TRIGGER ? absBedGap : (SLEEP_TRIGGER + gak::math::round<int>((absBedGap-SLEEP_TRIGGER) * POPULATION_CHANGE_BY_BEDS));
	const long bedsPopulationChange = bedGap >= 0 ? absBedsPopulationChange : -absBedsPopulationChange;

	const long freeJobs = m_theCity.getFreeJobs();
	const long unemployed = m_theCity.getUnemployed();
	const long jobPopulationChange = gak::math::round<long>(double(freeJobs-unemployed) * POPULATION_CHANGE_BY_JOBS);

	const double success = s_scale(getSuccessFactor());

	const long popuLationChange=gak::math::round<long>(double((bedsPopulationChange + jobPopulationChange)/2)*success);
	if( -popuLationChange >= gak::int64(m_theCity.population) )
		m_theCity.population = 1;
	else
		m_theCity.population += popuLationChange;

	long match = gak::math::min( freeJobs, unemployed );
	if( match > 0 )
	{
		match = gak::math::max( gak::math::round<int>(match * JOB_CHANGE), 1 );
		m_theCity.occupiedJobs += match;
	}
	if( unemployed > 0 )
	{
		m_theCity.money -= unemployed*UNEMPLOY_LOSS;
	}
	m_theCity.money += ((m_theCity.occupiedJobs * WORKER_TAX) - getCost());
	updateStatus();
}

void CityWindow::removeHouse( CityArea *area )
{
	CityArea::BuildingType building = area->buildingType;
	if( s_buildingInfos.hasElement( building ) )
	{
		const BuildingInfo &info = s_buildingInfos[building];
		m_theCity.jobs -= info.jobs;
		m_theCity.beds -= info.beds;
		m_theCity.powerValue -= info.powerReq;
		m_theCity.waterValue -= info.waterReq;

		--m_theCity.buildingCount[building];
	}
	area->buildingType = CityArea::btUNSPEC;
}

gak::int64 CityWindow::constructHouse2( CityArea *area, AreaID index, CityArea::BuildingType building )
{
	gak::int64 price = 0;
	assert( area == m_theCity.areas + index );
	if( CityArea::btUNSPEC != area->buildingType )
		removeHouse( area );

	AreaID transport=findGoodTransport( index );

	if( transport>= 0 && area->bits.test(CityArea::master) )
	{
		AreaID right = rightID( index );
		assert( right > 0 );
		AreaID bottom = bottomID( index );
		assert( bottom > 0 );
		AreaID bottomRight = rightID( bottom );
		assert( bottomRight > 0 );

		CityArea *next = m_theCity.areas + right;
		if( CityArea::btUNSPEC != next->buildingType )
			removeHouse( next );
		next = m_theCity.areas + bottom;
		if( CityArea::btUNSPEC != next->buildingType )
			removeHouse( next );
		next = m_theCity.areas + bottomRight;
		if( CityArea::btUNSPEC != next->buildingType )
			removeHouse( next );
	}
	if( transport>= 0 && s_buildingInfos.hasElement( building ) )
	{
		const BuildingInfo &info = s_buildingInfos[building];

		if( m_theCity.powerValue + info.powerReq <= 0 && m_theCity.waterValue + info.waterReq <= 0 )
		{
			m_theCity.jobs += info.jobs;
			m_theCity.beds += info.beds;
			m_theCity.powerValue += info.powerReq;
			m_theCity.waterValue += info.waterReq;
			++m_theCity.buildingCount[building];
			area->buildingType = building;
			price += info.price;
		}
	}
	return price;
}

void CityWindow::showToolHelp(AreaID index)
{
	if( checkValidID( index ) && index != m_lastToolHelp )
	{
		STRING						helpText;
		const CityArea				&area = m_theCity.areas[index];
		const CityArea::AreaType	areaType = area.areaType;
		if( CityArea::atUNSPEC != areaType )
		{
			const CityArea::BuildingType buildingType = area.buildingType;
			if( CityArea::btUNSPEC != buildingType && s_buildingInfos.hasElement( buildingType ) )
			{
				const BuildingInfo &info = s_buildingInfos[buildingType];
				const size_t count = m_theCity.buildingCount[buildingType];
				const size_t supportCount = info.supportCount * count;

				helpText = info.name;
				if( supportCount )
				{
					double successPercent = double(supportCount)/double(m_theCity.population) * 100;
					helpText
						.add(' ')
						.add(gak::NumberBuffer().addFloat(successPercent,0,1,0,',').c_str()).add('%');
				}
			}
			else if( s_areaInfos.hasElement(areaType) )
			{
				helpText = s_areaInfos[areaType].name;
			}
			if( !helpText.isEmpty() )
			{
				m_lastToolHelp = index;
				OverlappedWindow::showToolHelp( helpText );
			}
		}
	}
}

void CityWindow::updateStatus()
{
	m_moneyCount.setText(
		gak::NumberBuffer().addFloat( m_theCity.money/100.0, 0, 2, ' ', ',' ).add( " Ä" ).c_str()
	);
	m_populationCount.setText(
		gak::NumberBuffer().addNumber( m_theCity.population, 0, 0, ' ' ).c_str()
	);
	m_bedCount.setText(
		gak::NumberBuffer().addNumber( m_theCity.beds, 0, 0, ' ' ).c_str()
	);
	m_jobCount.setText(
		gak::NumberBuffer().addNumber( m_theCity.jobs, 0, 0, ' ' ).c_str()
	);
	m_occupiedJobsCount.setText(
		gak::NumberBuffer().addNumber( m_theCity.occupiedJobs, 0, 0, ' ' ).c_str()
	);

	m_powerCount.setText(
		gak::NumberBuffer().addNumber(m_theCity.powerValue, 0, 0, ' ' ).c_str()
	);
	m_waterCount.setText(
		gak::NumberBuffer().addNumber(m_theCity.waterValue, 0, 0, ' ' ).c_str()
	);

	m_workerCount.setText(
		gak::NumberBuffer().addNumber(m_theCity.getWorkers(), 0, 0, ' ' ).c_str()
	);

	m_successCount.setText(
		gak::NumberBuffer().addNumber(getSuccessFactor()*100, 0, 1, 0 ).addDigit('%').c_str()
	);


	invalidateWindow(false);
}

inline bool compatibleRoute2( CityArea::AreaType first, CityArea::AreaType second )
{
	if( CityArea::atCROSSING == first && (CityArea::atTRAMWAY == second || CityArea::atSTREET == second) )
		return true;

	return false;
}

inline bool compatibleRoute( CityArea::AreaType first, CityArea::AreaType second )
{
	return first == second || compatibleRoute2(first, second ) || compatibleRoute2(second, first);
}

Border CityWindow::calcBorder( AreaID index )
{
	assertValidID( index );

	Border result;

	memset( &result, 0, sizeof( result ) );
	int borderCount = 0;
	const CityArea				&me = m_theCity.areas[index];
	const CityArea::AreaType	myType = me.areaType;
	AreaID						otherIndex;
	if( (otherIndex = topID(index)) >= 0 && !compatibleRoute( myType, m_theCity.areas[otherIndex].areaType  ) )
	{
		result.top = 1;
		++borderCount;
	}
	if( (otherIndex = bottomID(index)) >= 0 && !compatibleRoute( myType, m_theCity.areas[otherIndex].areaType ) )
	{
		result.bottom = 1;
		++borderCount;
	}
	if( (otherIndex = leftID(index)) >= 0 && !compatibleRoute( myType, m_theCity.areas[otherIndex].areaType ) )
	{
		result.left = 1;
		++borderCount;
	}
	if( (otherIndex = rightID(index)) >= 0 && !compatibleRoute( myType, m_theCity.areas[otherIndex].areaType ) )
	{
		result.right = 1;
		++borderCount;
	}
	result.count = borderCount;

	return result;
}

void CityWindow::drawShape( Device &dev, int x, int y, const Border &border )
{
	if( border.count < 2)
	{
		dev.rectangle( x, y, x+AREA_PIXEL_SIZE+1, y+AREA_PIXEL_SIZE+1 );
	}
	else if( border.count == 2 )
	{
		if( border.right && border.bottom )
		{
			dev.pie( x,y, AREA_PIXEL_SIZE, M_PI/2, 0 );
		}
		else if( border.right && border.top )
		{
			y += AREA_PIXEL_SIZE+1;
			dev.pie( x,y, AREA_PIXEL_SIZE, 0, -M_PI/2 );
		}
		else if( border.left && border.bottom )
		{
			x += AREA_PIXEL_SIZE+1;
			dev.pie( x,y, AREA_PIXEL_SIZE, M_PI, M_PI/2 );
		}

		else if( border.top && border.left )
		{
			x += AREA_PIXEL_SIZE+1;
			y += AREA_PIXEL_SIZE+1;
			dev.pie( x,y, AREA_PIXEL_SIZE, -M_PI/2, M_PI );
		}
		else
		{
			dev.rectangle( x, y, x+AREA_PIXEL_SIZE+1, y+AREA_PIXEL_SIZE+1 );
		}
	}
	else if( border.count == 3 )
	{
		if( !border.top )
		{
			x += AREA_PIXEL_SIZE/2;

			dev.pie( x,y, AREA_PIXEL_SIZE/2, M_PI, 0 );
		}
		else if( !border.bottom )
		{
			x += AREA_PIXEL_SIZE/2;
			y += AREA_PIXEL_SIZE+1;
			dev.pie( x,y, AREA_PIXEL_SIZE/2, 0, M_PI );
		}
		else if( !border.left )
		{
			y += AREA_PIXEL_SIZE/2;
			dev.pie( x,y, AREA_PIXEL_SIZE/2, +M_PI/2, -M_PI/2 );
		}
		else if( !border.right )
		{
			x += AREA_PIXEL_SIZE+1;
			y += AREA_PIXEL_SIZE/2;
			dev.pie( x,y, AREA_PIXEL_SIZE/2, -M_PI/2, +M_PI/2 );
		}
		else
			dev.ellipse( x, y, x+AREA_PIXEL_SIZE+1, y+AREA_PIXEL_SIZE+1 );
	}
	else if( border.count == 4 )
	{
		dev.ellipse( x, y, x+AREA_PIXEL_SIZE+1, y+AREA_PIXEL_SIZE+1 );
	}
}

void CityWindow::drawArea( Device &dev, AreaID index, int x, int y )
{
	assertValidID(index);

	const CityArea &area = m_theCity.areas[index];
	if( area.bits.test(CityArea::dependent) && !area.bits.test(CityArea::master) )
		return;

	CityArea::AreaType type = area.areaType;
	Icon		*drawIcon = nullptr;
	COLORREF	color;
	COLORREF	shapeColor;
	COLORREF	dotColor;
	bool		hasShape = false, hasDot = false;

	drawIcon = s_buildingInfos.hasElement(area.buildingType) ? s_buildingInfos[area.buildingType].icon : nullptr; // area.icon;
	switch( type )
	{
	case CityArea::atCOMMERCIAL:
		color = colors::AQUA_MARINE;
		break;
	case CityArea::atINDUSTRY:
		color = RGB(64,64,64);
		break;
	case CityArea::atPUBLIC_BUILDING:
		color = RGB(198,198,198);
		if( area.buildingType == CityArea::btSCHOOL )
			drawIcon = area.bits.test(CityArea::master) ? &s_schoolL : &s_schoolS;
		else
			drawIcon = s_buildingInfos.hasElement(area.buildingType) ? s_buildingInfos[area.buildingType].icon : nullptr;
		break;
	case CityArea::atLIVING:
		color = colors::BEIGE;
		break;
	case CityArea::atCROSSING:
		drawIcon = &s_railCrossing;
		break;
	case CityArea::atSTREET:
		color = colors::GREEN;
		shapeColor = colors::GRAY;
		hasShape = true;
		break;
	case CityArea::atTRAMWAY:
		color = colors::GREEN;
		shapeColor = colors::BROWN;
		hasShape = true;
		break;
	case CityArea::atWATER:
		shapeColor = colors::BLUE;
		hasShape = true;
		dotColor = colors::WHITE;
		hasDot = true;
		color = colors::LIME_GREEN;
		break;
	case CityArea::atGREEN:
		dotColor = colors::GREEN;
		color = colors::LIME_GREEN;
		break;

		break;
	default:
		color = colors::WHITE;
	}

	dev.getBrush().create( color );
	dev.rectangle( x, y, x+AREA_PIXEL_SIZE+1, y+AREA_PIXEL_SIZE+1 );

	if( hasShape )
	{
		dev.getBrush().create( shapeColor );
		drawShape( dev, x, y, calcBorder( index ) );
	}
	else if( drawIcon )
	{
		Size si = drawIcon->getSize();
		if( area.bits.test(CityArea::master) )
			dev.drawIcon( x, y, AREA_PIXEL_SIZE*area.width, AREA_PIXEL_SIZE*area.height, *drawIcon );
		else if( !area.bits.test(CityArea::dependent) )
			dev.drawIcon( x, y, AREA_PIXEL_SIZE, AREA_PIXEL_SIZE, *drawIcon );
	}
	if( area.bits.test( CityArea::tree) )
		hasDot = true;
	else if( hasDot )
		hasDot = !randomNumber(WATER_RANDOM);
	if(hasDot)
	{
		// paint a dot
		dev.getBrush().create( dotColor );
		const int offset = AREA_PIXEL_SIZE/2-2;
		const int treeX = x + offset;
		const int treeY = y + offset;
		dev.ellipse( treeX, treeY, treeX+5, treeY+5 );
	}
}

inline bool compatibleBuilding( const CityArea *first, const CityArea *second )
{
	return first->areaType == second->areaType && (
		first->buildingType == second->buildingType || 
		CityArea::btUNSPEC == first->buildingType || 
		CityArea::btUNSPEC == second->buildingType
	);
}

void CityWindow::createLargeArea( CityArea *area, AreaID index )
{
	assertValidID(index);

	area->makeMaster();

	AreaID		right = rightID( index );
	CityArea	*rightArea = right >= 0 ? m_theCity.areas + size_t(right) : nullptr;

	AreaID		bottom = bottomID( index );
	CityArea	*bottomArea = right >= 0 ? m_theCity.areas + size_t(bottom) : nullptr;

	AreaID		bottomRight = rightID( bottom );
	CityArea	*bottomRightArea = bottomRight >= 0 ? m_theCity.areas + size_t(bottomRight) : nullptr;

	rightArea->bits.set(CityArea::dependent);
	bottomArea->bits.set(CityArea::dependent);
	bottomRightArea->bits.set(CityArea::dependent);
}

CityArea *CityWindow::checkLargeArea( AreaID *io_index ) const
{
	const AreaID	index = *io_index;
	assertValidID(index);
	const CityArea	&area = m_theCity.areas[index];

	// we must aggregate city buildings
	AreaID top = topID( index );
	const CityArea	*topArea = top >= 0 ? m_theCity.areas + size_t(top) : nullptr;

	AreaID left = leftID( index );
	const CityArea	*leftArea = left >= 0 ? m_theCity.areas + size_t(left) : nullptr;

	AreaID right = rightID( index );
	const CityArea	*rightArea = right >= 0 ? m_theCity.areas + size_t(right) : nullptr;

	AreaID bottom = bottomID( index );
	const CityArea	*bottomArea = bottom >= 0 ? m_theCity.areas + size_t(bottom) : nullptr;

	// check the top neighbors
	if( topArea && compatibleBuilding(topArea, &area) && !topArea->bits.test(CityArea::dependent) )
	{
		AreaID topLeft = leftID( top );
		const CityArea	*topLeftArea = topLeft >= 0 ? m_theCity.areas + size_t(topLeft) : nullptr;
		AreaID topRight = rightID( top );
		const CityArea	*topRightArea = topRight >= 0 ? m_theCity.areas + size_t(topRight) : nullptr;

		if( topLeftArea && compatibleBuilding(topLeftArea, &area) && !topLeftArea->bits.test(CityArea::dependent) &&
			leftArea && compatibleBuilding(leftArea, &area) && !leftArea->bits.test(CityArea::dependent) )
		{
			*io_index = topLeft;
/***/		return const_cast<CityArea*>(topLeftArea);
		}
		else if( topRightArea && compatibleBuilding(topRightArea, &area) && !topRightArea->bits.test(CityArea::dependent) &&
			rightArea && compatibleBuilding(rightArea, &area) && !rightArea->bits.test(CityArea::dependent) )
		{
			*io_index = top;
/***/		return const_cast<CityArea*>(topArea);
		}
	}

	// check the bottom neighbors if not yet found
	if( bottomArea && compatibleBuilding(bottomArea, &area) && !bottomArea->bits.test(CityArea::dependent) )
	{
		AreaID bottomLeft = leftID( bottom );
		const CityArea	*bottomLeftArea = bottomLeft >= 0 ? m_theCity.areas + size_t(bottomLeft) : nullptr;
		AreaID bottomRight = rightID( bottom );
		const CityArea	*bottomRightArea = bottomRight >= 0 ? m_theCity.areas + size_t(bottomRight) : nullptr;

		if( bottomLeftArea && compatibleBuilding(bottomLeftArea, &area) && !bottomLeftArea->bits.test(CityArea::dependent) &&
			leftArea && compatibleBuilding(leftArea, &area) && !leftArea->bits.test(CityArea::dependent) )
		{
			*io_index = left;
/***/		return const_cast<CityArea*>(leftArea);
		}
		else if( bottomRightArea && compatibleBuilding(bottomRightArea, &area) && !bottomRightArea->bits.test(CityArea::dependent) &&
			rightArea && compatibleBuilding(rightArea, &area) && !rightArea->bits.test(CityArea::dependent) )
		{
/***/		return const_cast<CityArea*>(&area);
		}
	}
	return nullptr;
}

void CityWindow::changeAreaType( AreaID index )
{
	if( m_theCity.money < 0 )
		return;

	if( checkValidID( index ) )
	{
		CityArea	&area = m_theCity.areas[index];
		if( CityArea::atUNSPEC == area.areaType )
		{
			CreateType selected = CreateType(m_creatSelector.getSelection());

			if(s_createAreaMap.hasElement(selected) )
			{
				CityArea::AreaType areaType = s_createAreaMap[selected];

				if( CityArea::atGREEN == areaType && randomNumber(TREE_RANDOM) < 1 )
					area.bits.set( CityArea::tree );
				const AreaInfo &areaInfo = s_areaInfos[areaType];
				m_theCity.money -= areaInfo.price;

				area.areaType = areaType;
				m_theCity.areaMapList[areaType].addElement( index );
				if(s_createBuildingMap.hasElement(selected) )
				{
					gak::int64 price = constructHouse( &area, index, s_createBuildingMap[selected] );
					if( price )
						m_theCity.money -= price;
					else
						area.areaType = CityArea::atUNSPEC;	// constructing the public building failed
				}
			}
			
			if( area.buildingType != CityArea::btUNSPEC )
			{
				AreaID masterIndex = index;
				CityArea *master =  checkLargeArea( &masterIndex );
				if( master != nullptr )
					createLargeArea( master, masterIndex );
			}
			else
			{
				m_areaQueue.push( index );
			}
			updateStatus();
		}
		else if(CityArea::atTRAMWAY == area.areaType )
		{
			CreateType selected = CreateType(m_creatSelector.getSelection());

			if(s_createAreaMap.hasElement(selected) && CityArea::atSTREET == s_createAreaMap[selected])
			{
				const AreaInfo &areaInfo = s_areaInfos[CityArea::atCROSSING];
				m_theCity.money -= areaInfo.price;
				area.areaType = CityArea::atCROSSING;
				updateStatus();
			}
		}
		else if(CityArea::atSTREET == area.areaType )
		{
			CreateType selected = CreateType(m_creatSelector.getSelection());

			if(s_createAreaMap.hasElement(selected) && CityArea::atTRAMWAY == s_createAreaMap[selected])
			{
				const AreaInfo &areaInfo = s_areaInfos[CityArea::atCROSSING];
				m_theCity.money -= areaInfo.price;
				area.areaType = CityArea::atCROSSING;
				updateStatus();
			}
		}
	}
}

// --------------------------------------------------------------------- //
// ----- class protected ----------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- class virtuals ------------------------------------------------ //
// --------------------------------------------------------------------- //

bool StadtSimulationApp::startApplication( HINSTANCE /*hInstance*/, const char * /*cmdLine*/ )
{
	doEnableLogEx(gakLogging::llInfo);
	doDisableLog();
	setApplication("StadtSimulation");
	setCompany("gak");

	s_police		= loadIcon( IDI_POLICE );
	s_fire			= loadIcon( IDI_FIRE );
	s_hospital		= loadIcon( IDI_HOSPITAL );
	s_townHall		= loadIcon( IDI_TOWN_HALL );
	s_voltaic		= loadIcon(IDI_VOLTAIC);
	s_waterPlant	= loadIcon(IDI_WATER_PLANT);

	s_schoolS		= loadIcon( IDI_SMALL_SCHOOL );
	s_schoolL		= loadIcon( IDI_LARGE_SCHOOL );

	s_livingS		= loadIcon( IDI_SMALL_LIVING );
	s_livingL		= loadIcon( IDI_LARGE_LIVING );

	s_commercialS	= loadIcon( IDI_SMALL_COMMERCIAL );
	s_commercialL	= loadIcon( IDI_LARGE_COMMERCIAL );

	s_industryS		= loadIcon( IDI_SMALL_INDUSTRY );
	s_industryL		= loadIcon( IDI_LARGE_INDUSTRY );

	s_railCrossing	= loadIcon( IDI_RAIL_CROSSING );

	return 0;
}

ProcessStatus CityWindow::handleCreate()
{
	const int COMBO_WIDTH = 300;
	const int COMBO_HEIGHT = 250;
	const int PADDING = 4;
	const int INFO_X = 150;

	m_tools.create( this, COMBO_WIDTH, 0, "Stadtstatus" );
	int y = 0;
	m_creatSelector.create( &m_tools, 0, y, COMBO_WIDTH, COMBO_HEIGHT );
	fillSelector( &m_creatSelector, createTypes );
	m_creatSelector.selectEntry( 0 );
	y += m_creatSelector.getSize().height+PADDING;

	m_moneyLabel.setText("Vermˆgen");
	m_moneyLabel.create( &m_tools, PADDING, y, 0, 0 );
	m_moneyCount.create( &m_tools, INFO_X, y, 0, 0 );
	y += m_moneyLabel.getSize().height+PADDING;

	m_populationLabel.setText("Einwohner");
	m_populationLabel.create( &m_tools, PADDING, y, 0, 0 );
	m_populationCount.create( &m_tools, INFO_X, y, 0, 0 );
	y += m_populationLabel.getSize().height+PADDING;

	m_bedLabel.setText("Schlafpl‰tze");
	m_bedLabel.create( &m_tools, PADDING, y, 0, 0 );
	m_bedCount.create( &m_tools, INFO_X, y, 0, 0 );
	y += m_bedLabel.getSize().height+PADDING;

	m_jobLabel.setText("Arbeitspl‰tze");
	m_jobLabel.create( &m_tools, PADDING, y, 0, 0 );
	m_jobCount.create( &m_tools, INFO_X, y, 0, 0 );
	y += m_jobLabel.getSize().height+PADDING;

	m_occupiedJobsLabel.setText("   Besetzte");
	m_occupiedJobsLabel.create( &m_tools, PADDING, y, 0, 0 );
	m_occupiedJobsCount.create( &m_tools, INFO_X, y, 0, 0 );
	y += m_occupiedJobsLabel.getSize().height;

	m_powerLabel.setText("Energie");
	m_powerLabel.create( &m_tools, PADDING, y, 0, 0 );
	m_powerCount.create( &m_tools, INFO_X, y, 0, 0 );
	y += m_powerLabel.getSize().height+PADDING;

	m_waterLabel.setText("Wasser");
	m_waterLabel.create( &m_tools, PADDING, y, 0, 0 );
	m_waterCount.create( &m_tools, INFO_X, y, 0, 0 );
	y += m_waterLabel.getSize().height+PADDING;

	m_separator.setText( "___________________________________________" );
	m_separator.create( &m_tools, 0, y, 0, 0 );
	y += m_separator.getSize().height+PADDING;

	m_workerLabel.setText("Arbeitnehmer");
	m_workerLabel.create( &m_tools, PADDING, y, 0, 0 );
	m_workerCount.create( &m_tools, INFO_X, y, 0, 0 );
	y += m_workerLabel.getSize().height+PADDING;

	m_successLabel.setText("Zufriedenheit");
	m_successLabel.create( &m_tools, PADDING, y, 0, 0 );
	m_successCount.create( &m_tools, INFO_X, y, 0, 0 );
	y += m_successLabel.getSize().height+PADDING;

	m_tools.resize( COMBO_WIDTH, y );
	m_tools.adjustWindoRect();

	try
	{
		gak::readFromBinaryFile( FILE_NAME, &m_theCity, magic, version, false );
	}
	catch( ... )
	{
		gak::strRemove(FILE_NAME);
	}

	updateStatus();

	setTimer(1000);

	return psDO_DEFAULT;
}

SuccessCode CityWindow::handleClose()
{
	gak::writeToBinaryFile(
		FILE_NAME, m_theCity,
		magic, version,
		gak::owmOverwrite
	);
	return OverlappedWindow::handleClose();
}

ProcessStatus CityWindow::handleMouseMove( ModifierFlags modifier, const Point &position )
{
	AreaID index = idForPoint( position );
	if( checkValidID( index ) )
	{
		if( modifier.test( ModifierFlags::mbLeftMouseButton ) )
		{
			changeAreaType( index );
		}
		else
		{
			showToolHelp(index);
		}
	}
	return psPROCESSED;
}

ProcessStatus CityWindow::handleLeftButton( LeftButton , ModifierFlags , const Point &position )
{
	AreaID index = idForPoint( position );
	changeAreaType( index );
	return psPROCESSED;
}

ProcessStatus CityWindow::handleRepaint( Device &hDC )
{
	Size			size = getClientSize();
	MemoryDevice	mem( hDC, size );

	mem.getPen().setStyle(Pen::psNull);
	size_t	line = 0;
	AreaID	id=0;
	int		x = 0, y=0;
	while( true )
	{
		if( x > size.width || x >= CITY_SIZE*AREA_PIXEL_SIZE)
		{
			x = 0;
			++line;
			id = AreaID(line*CITY_SIZE);
			y = int(line*AREA_PIXEL_SIZE);
		}
		if( y > size.width || y >= CITY_SIZE*AREA_PIXEL_SIZE )
		{
			break;
		}

		drawArea(mem, id, x, y );
		x += AREA_PIXEL_SIZE;
		++id;
	}

	mem.drawToWindow();

	return psPROCESSED;
}

void CityWindow::handleTimer()
{
	if( m_areaQueue.size() )
	{
		AreaID areaID = m_areaQueue.pop();
		if( !processArea( areaID ) )
			m_areaQueue.push(areaID);
	}
	else
	{
		selectRandomArea( CityArea::atLIVING );
		selectRandomArea( CityArea::atCOMMERCIAL );
		selectRandomArea( CityArea::atINDUSTRY );
	}
	nextCityStep();
}

// --------------------------------------------------------------------- //
// ----- class publics ------------------------------------------------- //
// --------------------------------------------------------------------- //

// --------------------------------------------------------------------- //
// ----- entry points -------------------------------------------------- //
// --------------------------------------------------------------------- //

#ifdef __BORLANDC__
#	pragma option -RT.
#	pragma option -b.
#	pragma option -a.
#	pragma option -p.
#endif

