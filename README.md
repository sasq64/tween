
## C++11 Tween engine

First of all, `Tween::update(double t)` needs to be called regularly. Normally you do it once every render loop,
but you can also safely do it from another thread:

```c++
std::thread tweenThread([]() {
	auto t0 = getms();
	while(run) {
		float seconds = (double)(getms()-t0) / 1000.0;
		Tween::updateTweens(seconds);
		sleepms(50);
	}
});
```

Here is how to create a simple tween that interpolates a variable from 0 to 100 over one second:

```c++
float x = 0;
Tween::make().to(x, 100.0).seconds(1.0);	
```

It will use the default *smoothstep* tweening function and start immediately after the whole expression is evaluated.

You don't need to tween an existing value, but if you don't, you probably want
a callback when the value changes:

```c++
Tween::make().fromTo(200, 0).linear().onUpdate([](int x) {
	printf("%d\n", x);
});
```

You can tween anything that is either an arithmetic value or something that looks like a *std::vector*;
meaning anything that can be indexed with `[]` and has a `size()` method.

This example uses the *vec2f* class to tween a position in from the left of the screen, delay for a second, then tween out to the right;

```c++
vec2f textPos(10, 100);
Tween::make().from(textPos, vec2f(-500, 100)).easeInBack().onComplete([]() {
	Tween::make().to(textPos, vec2f(1500, 100)).easeOutBack().delay(1.0);
});
```

Many methods affect only the latest tweened value in the expression, and in fact are not even possible unless a `to()` or `from()` method
has been called. Also, the `onUpdate()` callback can take 0 or more arguments, but the first argument must be of the same type as the
last tweened value:

```c++
Color red { 0xffff0000 };

Tween::make().delay(1.0).to(0xffffffff).seconds(1.0); 
// ^^ Wont compile, delay() needs to be called after to()

auto handleColor = [](int color) {
	// Do something with color
};

Tween::make().to(0xffffffff).delay(1.0).onUpdate(handleColor).seconds(1.0);
// ^^ Wont compile, handleColor must take 0 arguments, or a type Color
```

If you need to control the tween, you can save the result of the tween expression, but then it will not start automatically;

```c++
Tween myTween = Tween::make().from(x, 1.0).sine().repeating().seconds(0.5);
myTween.start()
...
myTween.cancel()
```

When tweening members of an object, a good habit is to tell the tween about it, so it can retain a reference to it during the
tween;

```c++

auto mySprite = std::make_shared<Sprite>(0,0);
Tween::make().retain(mySprite).to(mySprite->xpos, 100.0).seconds(2.5);
mySprite = nullptr; // Safe, shared_ptr still held by the Tween
```
