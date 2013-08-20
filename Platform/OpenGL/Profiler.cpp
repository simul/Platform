#include <GL/glew.h>
#include <sstream>
#include "Profiler.h"
#include "SimulGLUtilities.h"
using std::string;
using std::map;
bool enabled=false;
using namespace simul;
using namespace opengl;

Profiler Profiler::GlobalProfiler;

Profiler &Profiler::GetGlobalProfiler()
{
	return GlobalProfiler;
}

void Profiler::Uninitialize()
{
	profiles.clear();
    enabled=false;
}

void Profiler::Initialize(void*)
{
    //enabled=true;
}

void Profiler::StartProfile(const std::string& name)
{
    if(!enabled)
        return;
    ProfileData& profileData = profiles[name];
    _ASSERT(profileData.QueryStarted == false);
    if(profileData.QueryFinished!= false)
        return;
    if(profileData.TimestampQuery[0] == 0)
    {
		glGenQueries(QueryLatency,profileData.TimestampQuery);
		for(int i=0;i<QueryLatency;i++)
		{
			//glQueryCounter(profileData.TimestampQuery[i], GL_TIMESTAMP);
		}
    }
ERROR_CHECK
	glBeginQuery(GL_TIME_ELAPSED,profileData.TimestampQuery[currFrame]);
    profileData.QueryStarted = true;
ERROR_CHECK
}

void Profiler::EndProfile(const std::string& name)
{
    if(!enabled)
        return;
    ProfileData& profileData = profiles[name];
	glEndQuery(GL_TIME_ELAPSED);//,profileData.TimestampQuery[currFrame]);
	//glQueryCounter(profileData.TimestampEndQuery[currFrame], GL_TIMESTAMP);
    if(profileData.QueryStarted != true)
		return;
    _ASSERT(profileData.QueryFinished == false);
    profileData.QueryStarted = false;
    profileData.QueryFinished = true;
}

template<typename T> inline std::string ToString(const T& val)
{
    std::ostringstream stream;
    if (!(stream << val))
        throw std::runtime_error("Error converting value to string");
    return stream.str();
}

void Profiler::EndFrame()
{
    if(!enabled)
        return;

	unsigned int lastFrame=currFrame;
    currFrame = (currFrame + 1) % QueryLatency;    

    float queryTime = 0.0f;
	output="";
    // Iterate over all of the profiles
    ProfileMap::iterator iter;
    for(iter = profiles.begin(); iter != profiles.end(); iter++)
    {
        ProfileData& profileData = (*iter).second;
        if(profileData.QueryFinished == false)
            continue;
        profileData.QueryFinished = false;
        timer.UpdateTime();
		GLint stopTimerAvailable = 0;
		while (!stopTimerAvailable)
		{
			glGetQueryObjectiv(profileData.TimestampQuery[lastFrame], 
									   GL_QUERY_RESULT_AVAILABLE, 
									   &stopTimerAvailable);
		}
        // Get the query data
        GLuint64 startTime = 0;
        //GLuint64 endTime = 0;
		// get query results
		glGetQueryObjectui64v(profileData.TimestampQuery[lastFrame], GL_QUERY_RESULT, &startTime);
		//glGetQueryObjectui64v(profileData.TimestampEndQuery[lastFrame], GL_QUERY_RESULT, &endTime);
	 
        timer.UpdateTime();
        queryTime += timer.Time;

        double time = (startTime) / 1000000.0;//endTime - 
        output+= (*iter).first + ": " + ToString(time) + "ms\n";
        iter->second.time=(float)time;
    }
    output+= "Time spent waiting for queries: " + ToString(queryTime) + "ms";
}

float Profiler::GetTime(const std::string &name) const
{
	if(!enabled)
		return 0.f;
	return profiles.find(name)->second.time;
}


// == ProfileBlock ================================================================================

ProfileBlock::ProfileBlock(const std::string& name) : name(name)
{
    Profiler::GetGlobalProfiler().StartProfile(name);
}

ProfileBlock::~ProfileBlock()
{
    Profiler::GetGlobalProfiler().EndProfile(name);
}

float ProfileBlock::GetTime() const
{
	return Profiler::GetGlobalProfiler().GetTime(name);
}


Profiler::ProfileData::~ProfileData()
{
	glDeleteQueries(Profiler::QueryLatency,TimestampQuery);
}