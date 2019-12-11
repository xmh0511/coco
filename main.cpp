#include <iostream>
#include "coco.hpp"

void func0() {
	int v = 2048;
	std::cout << "in func0 "<< v << std::endl;
	auto current = coco::add_task();
	std::thread t0([current]() {
		std::this_thread::sleep_for(std::chrono::seconds(3));
		coco::resume(current);
	});
	t0.detach();
	coco::co_yiled();
	std::cout << "I am back "<<v << std::endl;
}
void func1() {
	std::cout << "in func1" << std::endl;
	coco::co_yiled();
	std::cout << "out func1" << std::endl;
}

int main() {
	auto ctx = coco::co_create(func0);
	auto ctx1 = coco::co_create(func1);
	coco::co_resume(ctx);
	coco::co_resume(ctx1);
	std::cout << "abc" << std::endl;
	coco::co_resume(ctx1);
	coco::co_wait();
}
