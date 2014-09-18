#include "tween.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace tween {

std::vector<std::shared_ptr<TweenImpl>> Tween::allTweens;
double Tween::currentTime = 0;
std::mutex Tween::tweenMutex;

double Tween::linear_fn(double t) {
	return t;
}

double Tween::smoothStep_fn(double t) {
	return (t*t *(3 - 2*t));
}

double Tween::easeInSine_fn (double t) {
	return 1 - cos(t * (M_PI/2));
}

double Tween::sine_fn(double t) {
	return (sin(t * (M_PI*2) - M_PI/2) + 1.0)/2.0;        
}

double Tween::easeOutSine_fn(double t) {
	return sin(t * (M_PI/2));        
}

double Tween::easeInOutSine_fn(double t) {
	return -0.5 * (cos(M_PI*t) - 1);
}

double Tween::easeInBack_fn (double t) {
	double s = 1.70158f;
	return t*t*((s+1)*t - s);
}

double Tween::easeOutBack_fn(double t) {        
	double s = 1.70158f;
	t--;
	return (t*t*((s+1)*t + s) + 1);
}

double Tween::easeInOutBack_fn(double t) {
	double s = 1.70158f * 1.525f;
	//double s2 = s * 1.525f;
	t *= 2;
	if (t < 1) return 1.0/2*(t*t*((s+1)*t - s));
	double postFix = t-=2;
	return 1.0/2*((postFix)*t*((s+1)*t + s) + 2);
}

TweenImpl::~TweenImpl() {
	if(!isTweening) {
		Tween::addTween(*this);
	}
}

Tween Tween::make() {
	return Tween(std::make_shared<TweenImpl>(currentTime, smoothStep_fn, false));
}

void Tween::addTween(const TweenImpl& ti) {
	std::lock_guard<std::mutex> guard(tweenMutex);
	allTweens.push_back(std::make_shared<TweenImpl>(ti));
	allTweens.back()->isTweening = true;
}

void Tween::cancel() {
	std::lock_guard<std::mutex> guard(tweenMutex);

	auto it = Tween::allTweens.begin();
	while(it != Tween::allTweens.end()) {
		if(*it == impl) {
			Tween::allTweens.erase(it);
			break;
		}
		++it;
	}
}

bool TweenImpl::step() {
		size_t ended = 0;
		Tween tween;
		for(auto &a : args) {
			float t = (float)((Tween::currentTime - startTime - a->delay) / totalTime);
			if(t < 0.0)
				continue;
			if(t > 1.0) {
				if(do_rep)
					t -= 1.0;
				else if(backto) {
					ended++;
					a->set(0.0, tween);
				} else {
					ended++;
					a->set(1.0, tween);
					continue;
				}
			}
			a->set(tween_func(t), tween);
		}
		return ended < args.size();
	}

int Tween::updateTweens(double t) {

	std::lock_guard<std::mutex> guard(tweenMutex);

	static std::vector<std::shared_ptr<TweenImpl>> doneTweens;

	currentTime = t;
	auto it = allTweens.begin();
	while(it != allTweens.end()) {
		if(!(*it)->step()) {
			doneTweens.push_back(*it);
			it = allTweens.erase(it);
		} else {
			it++;
		}
	}

	for(auto &dt : doneTweens) {
		Tween tw;
		dt->on_complete_cb.call(tw, 1.0);
	}
	doneTweens.clear();

	return (int)allTweens.size();
}

Tween::Tween(int dummy) : impl(std::make_shared<TweenImpl>(currentTime, smoothStep_fn)) {
}

Tween::Tween(std::shared_ptr<TweenImpl> i) : impl(i) {
}

void Tween::finish() {
	for(auto &a : impl->args) {
		a->set(1.0, *this);
	}
	cancel();
}

Tween &Tween::seconds(float s) {
	impl->totalTime = s;
	return *this;
}

Tween &Tween::speed(float s) {
	impl->dspeed = s;
	return *this;
}

Tween &Tween::linear() {
	impl->tween_func = linear_fn;
	return *this;
}

Tween &Tween::smoothstep() {
	impl->tween_func = smoothStep_fn;
	return *this;
}

Tween &Tween::easeinback() {
	impl->tween_func = easeInBack_fn;
	return *this;
}

Tween &Tween::easeoutback() {
	impl->tween_func = easeOutBack_fn;
	return *this;
}

Tween &Tween::easeinsine() {
	impl->tween_func = easeInSine_fn;
	return *this;
}

Tween &Tween::easeoutsine() {
	impl->tween_func = easeOutSine_fn;
	return *this;
}

Tween &Tween::sine() {
	impl->tween_func = sine_fn;
	impl->backto = true;
	return *this;
}

Tween &Tween::repeating() {
	impl->do_rep = true;
	return *this;
}

void Tween::start() {
	if(!impl->isTweening) {
		impl->startTime = currentTime;
		std::lock_guard<std::mutex> guard(tweenMutex);
		allTweens.push_back(impl);
		allTweens.back()->isTweening = true;
	}
}

} // namespace tween

#if (defined UNIT_TEST || defined TWEEN_UNIT_TEST)

#include "catch.hpp"

#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <mutex>
#include <future>
#include <atomic>

void sleepms(uint ms) {
#ifdef WIN32
	Sleep(ms);
#else
	usleep(ms*1000);
#endif
}

uint64_t getms() {
	timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000 + tv.tv_usec/1000);
}

TEST_CASE("tween::basic", "Basic tween") {

	using tween::Tween;

	struct { int score = 0; short energy = 0; } demo;

	auto showScore = [&](int score) {
		printf("SCORE:%d\n", score);
	};

	auto showFood = [](int food, Tween t, double v) -> int {
		printf("FOOD:%d (%f)\n", food, v);
		return 0;
	};

	Tween myt = Tween::make().linear().to(demo.score, 10).onUpdate(showScore).from(demo.energy, 250).onUpdate(showFood).seconds(2);

	double t = 0;
	for(int i=0; i<10; ++i) {
		Tween::updateTweens(t += 0.1);
	}
	REQUIRE(demo.score == 0);
	REQUIRE(demo.energy == 250);

	myt.start();	
	for(int i=0; i<10; ++i) {
		Tween::updateTweens(t += 0.1);
	}

	REQUIRE(demo.score == 5);
	REQUIRE(demo.energy == 125);
	for(int i=0; i<50; ++i)
		Tween::updateTweens(t += 0.1);

	REQUIRE(demo.score == 10);
	REQUIRE(demo.energy == 0);

	std::vector<float> v = { 0, 1, 10, 100 };
	Tween::make().to(v, {4,4,4,4}).seconds(4.0);
	for(int i=0; i<10; ++i)
		Tween::updateTweens(t += 0.1);
	REQUIRE(v[0] == 0.625);
	REQUIRE(v[3] == 85);
	for(int i=0; i<30; ++i)
		Tween::updateTweens(t += 0.1);
	REQUIRE(v[1] == 4);
	REQUIRE(v[2] == 4);

	Tween::make().fromTo(10,20).onUpdate([](int x) { printf("%d\n", x); }).seconds(1);
	for(int i=0; i<20; ++i)
		Tween::updateTweens(t += 0.1);

	REQUIRE(Tween::updateTweens(t) == 0);
}

TEST_CASE("tween::thread", "Tween threading") {

	using tween::Tween;

	std::mutex m;

	std::atomic<bool> run;
	std::atomic<int> total;
	std::atomic<int> done;
	done = 0;
	run = true;
	total = 0;

	Tween::updateTweens(0.0);

	auto l = [&run]() {
		auto t0 = getms();
		while(run) {
			auto t1 = getms();
			float t = (double)(t1-t0) / 1000.0;
			Tween::updateTweens(t);
			sleepms(2);
		}
	};
	std::thread tweenThread(l);

	const int ITERATIONS = 500;
	const int TOTAL = (2+3+4+5)*ITERATIONS;

	for(int i=0; i<ITERATIONS; i++) {
		Tween::make().fromTo(1,5).onUpdate([&](int x) { total += x; }).onComplete([&]() { done++; }).seconds(1);
		sleepms(i % 10);
	}

	printf("WAITING FOR TWEENS\n");
	sleepms(1500);
	run = false;
	printf("JOINING\n");
	tweenThread.join();
	std::lock_guard<std::mutex> guard(m);
	int tot = total;
	int dn = done;
	REQUIRE(tot == TOTAL);
	REQUIRE(dn == ITERATIONS);
}

#endif