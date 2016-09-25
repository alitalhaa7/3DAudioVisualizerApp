/*
Made By Talha Ali
Using C++, OpenGL and GLSL.
Built upon open source library, Cinder.

Particle placement is based off of sample found within Cinder library. I've added audio detection, music analysis 
and particle movement based on music.

19-09-2016

*/



#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/TextureFont.h"
#include "cinder/audio/audio.h"

#include "cinder/audio/Context.h"
#include "cinder/audio/MonitorNode.h"
#include "cinder/audio/Utilities.h"
#include "AudioDrawUtils.h"


#include "Resources.h"

#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/app/RendererGl.h"

#include "cinder/audio/audio.h"

#include "Resources.h"
#include "AudioDrawUtils.h"
#include "cinder/Rand.h"
#include "glm\gtc\random.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ci::gl;







#if defined( CINDER_GL_ES ) 
const int MaxParticles = 600e2;
#else
const int MaxParticles = 600e3;
#endif





class AudioVisualsApp : public App {
  public:
	void setup() override;
	void resize() override;
	void update() override;
	void draw() override;
	void animate();
	void drawLabels();
	void printBinInfo(int mouseX);
	void keyDown(KeyEvent event);

	void fileDrop(FileDropEvent event) override;


	std::shared_ptr<audio::GenSineNode> a;
	audio::GainNodeRef				mGain;
	audio::BufferPlayerNodeRef		mBufferPlayerNode;
	WaveformPlot				mWaveformPlot;
	audio::InputDeviceNodeRef		mInputDeviceNode;
	audio::MonitorSpectralNodeRef	mMonitorSpectralNode;

	vector<float>					mMagSpectrum;
	SpectrumPlot					mSpectrumPlot;
	float maxmag = 0;
	float minmag = 15;
	CameraPersp		mCam;
	geom::Sphere		sphere;
	gl::BatchRef		mSphere;
	vec3 offset;
	Color sphereColor;
	vec3 scalefactor;	
	int mouseX;
	float time;

	Color bgColor;


	bool spacebuttonpressed=false;
	bool songdragged = false;

private:
	gl::GlslProgRef mRenderProg;
	gl::GlslProgRef mUpdateProg;

	gl::VaoRef		mAttributes[2];
	gl::VboRef		pbuffer[2];

	std::uint32_t	InitialIndex = 0;
	std::uint32_t	FinalIndex= 1;

	float			mMouseForce = 0.0f;
	vec3			mMousePos = vec3(0, 0, 0);
	float Force = 0.0f;

	audio::SourceFileRef sourceFile;


	Font mFont;
	gl::TextureFontRef mTextureFont;


};

//This is a struct holding all the data required in order to create the particles
struct ParticleData
{
	vec3	position; //3 field vector representing position of particles during "rest" state.
	vec3	particleposition;//3 Field Vector represnting position of particles during "disturbance"
	vec3	home;
	ColorA  color;//Color of particles
	float	damping;//The damping effect
};





void AudioVisualsApp::setup()
{
	getWindow()->setTitle("Audio Visuals by Talha Ali");

	mFont = Font("Arial", 22);

	mTextureFont = gl::TextureFont::create(mFont);

	vector<ParticleData> particles;
	particles.assign(MaxParticles, ParticleData());
    float azimuth = 256.0f * M_PI / particles.size();
	float inclination = M_PI / particles.size();
	float radius = 200.0f;
	for (int i = 0; i < particles.size(); ++i)
	{	
		float x = radius * sin(inclination * i) * cos(azimuth * i);
		float y = radius * cos(inclination * i);
		float z = radius * sin(inclination * i) * sin(azimuth * i);

		auto &p = particles.at(i);
		p.position = vec3(getWindowCenter() + vec2(0.0f, 40.0f), 0.0f) +vec3(x, y, z);
		p.home = p.position;
		p.particleposition = p.home;
		p.damping = Rand::randFloat(0.965f, 0.985f);
		p.color = Color(CM_HSV, lmap<float>(i, 0.0f, particles.size(), 0.04f, 1.0f), 1.0f, 1.0f);
	}


	pbuffer[InitialIndex] = gl::Vbo::create(GL_ARRAY_BUFFER, particles.size() * sizeof(ParticleData), particles.data(), GL_STATIC_DRAW);
	pbuffer[FinalIndex] = gl::Vbo::create(GL_ARRAY_BUFFER, particles.size() * sizeof(ParticleData), nullptr, GL_STATIC_DRAW);

	mRenderProg = gl::getStockShader(gl::ShaderDef().color());

	for (int i = 0; i < 2; ++i)
	{	
		mAttributes[i] = gl::Vao::create();
		gl::ScopedVao vao(mAttributes[i]);

		gl::ScopedBuffer buffer(pbuffer[i]);
		gl::enableVertexAttribArray(0);
		gl::enableVertexAttribArray(1);
		gl::enableVertexAttribArray(2);
		gl::enableVertexAttribArray(3);
		gl::enableVertexAttribArray(4);
		gl::vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, position));
		gl::vertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, color));
		gl::vertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, particleposition));
		gl::vertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, home));
		gl::vertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleData), (const GLvoid*)offsetof(ParticleData, damping));
	}

	

#if defined( CINDER_GL_ES_3 )
	mUpdateProg = gl::GlslProg::create(gl::GlslProg::Format().vertex(loadAsset("particleUpdate_es3.vs"))
		.fragment(loadAsset("no_op_es3.fs"))
#else
	mUpdateProg = gl::GlslProg::create(gl::GlslProg::Format().vertex(loadAsset("particleUpdate.vs"))
#endif
		.feedbackFormat(GL_INTERLEAVED_ATTRIBS)
		.feedbackVaryings({ "position", "pposition", "home", "color", "damping" })
		.attribLocation("iPosition", 0)
		.attribLocation("iColor", 1)
		.attribLocation("iPPosition", 2)
		.attribLocation("iHome", 3)
		.attribLocation("iDamping", 4)
		);

	








	auto ctx = audio::Context::master();
	sourceFile = audio::load(loadResource(music), ctx->getSampleRate());

	//audio::BufferRef buffer = sourceFile->loadBuffer();
	/*mBufferPlayerNode = ctx->makeNode(new audio::BufferPlayerNode(buffer));




	auto monitorFormat = audio::MonitorSpectralNode::Format().fftSize(2048).windowSize(1024);
	mMonitorSpectralNode = ctx->makeNode(new audio::MonitorSpectralNode(monitorFormat));
	mBufferPlayerNode >> mMonitorSpectralNode >> ctx->getOutput();;
	ctx->enable();
	*/
	bgColor = Color(0.0, 0.0, 0.0);
	

}







void AudioVisualsApp::fileDrop(FileDropEvent event)
{

	auto ctx = audio::Context::master();

	fs::path filePath = event.getFile(0);
	sourceFile = audio::load(loadFile(filePath));
	audio::BufferRef buffer = sourceFile->loadBuffer();
	mBufferPlayerNode = ctx->makeNode(new audio::BufferPlayerNode(buffer));
	auto monitorFormat = audio::MonitorSpectralNode::Format().fftSize(2048).windowSize(1024);
	mMonitorSpectralNode = ctx->makeNode(new audio::MonitorSpectralNode(monitorFormat));
	mBufferPlayerNode >> mMonitorSpectralNode >> ctx->getOutput();;
	ctx->enable();
	songdragged = true;
	
}





void AudioVisualsApp::resize(){
	if (mBufferPlayerNode)
		mWaveformPlot.load(mBufferPlayerNode->getBuffer(), getWindowBounds());
}


void AudioVisualsApp::update()
{
	
	gl::ScopedGlslProg prog(mUpdateProg);
	gl::ScopedState rasterizer(GL_RASTERIZER_DISCARD, true);	// turn off fragment stage
	mUpdateProg->uniform("Disturbance", Force);
	mUpdateProg->uniform("DisturbancePosition", glm::ballRand(180.0f)+vec3(getWindowCenter().x,getWindowCenter().y,0));

	gl::ScopedVao source(mAttributes[InitialIndex]);
	gl::bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, pbuffer[FinalIndex]);
	gl::beginTransformFeedback(GL_POINTS);

	gl::drawArrays(GL_POINTS, 0, MaxParticles);

	gl::endTransformFeedback();

	std::swap(InitialIndex, FinalIndex);


	mSpectrumPlot.setBounds(Rectf(40, 40, (float)getWindowWidth() - 40, (float)getWindowHeight() - 40));
	if (songdragged){
		mMagSpectrum = mMonitorSpectralNode->getMagSpectrum();
		printBinInfo(350);
		draw();
	}


}




void AudioVisualsApp::keyDown(KeyEvent event){
	if (event.getCode() == KeyEvent::KEY_SPACE) {
		

			spacebuttonpressed = true;
		if (songdragged){
			if (mBufferPlayerNode->isEnabled())
				mBufferPlayerNode->stop();
			else
				mBufferPlayerNode->start();
		}
	}
}



void AudioVisualsApp::draw()
{


	gl::clear(bgColor);
	gl::setMatricesWindowPersp(getWindowSize());
	gl::enableDepthRead();
	gl::enableDepthWrite();
	
	gl::ScopedGlslProg render(mRenderProg);
	gl::ScopedVao vao(mAttributes[InitialIndex]);
	gl::context()->setDefaultShaderVars();


	if (!songdragged){
		mTextureFont->drawString("Audio Visuals by Talha Ali. Built on the open source library Cinder. Particle position and color is inspired by Cinder sample.", vec2(10.0f, 40.0f));

		mTextureFont->drawString("Drag and drop any music file (.mp3, .mp4, etc.) and press the space button.", vec2(10.0f, 70.0f));

	
	}

	gl::drawArrays(GL_POINTS, 0, MaxParticles);


}



void AudioVisualsApp::printBinInfo(int mouseX)
{
	size_t numBins = mMonitorSpectralNode->getFftSize() / 2;
	size_t bin = std::min(numBins - 1, size_t((numBins * (mouseX - mSpectrumPlot.getBounds().x1)) / mSpectrumPlot.getBounds().getWidth()));

	float binFreqWidth = mMonitorSpectralNode->getFreqForBin(1) - mMonitorSpectralNode->getFreqForBin(0);
	float freq = mMonitorSpectralNode->getFreqForBin(bin);
	float mag = audio::linearToDecibel(mMagSpectrum[bin]);
	if (mag > maxmag){
		maxmag = mag;
	}
	if (mag < minmag){
		minmag = mag;

	}

	float r = (mag / maxmag);
	
	sphereColor = Color(CM_RGB, r, 0.0, 0.0);
	scalefactor = vec3(r);



	Force = mag*8;
	
	
}

CINDER_APP(AudioVisualsApp, RendererGl(RendererGl::Options().msaa(8)), [](App::Settings *settings) {
	settings->setWindowSize(1280, 720);
	settings->setMultiTouchEnabled(false);
})