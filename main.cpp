#include <iostream>
#include <setjmp.h>
#include <vector>
#include <functional>
#include <cstring>
#include <thread>

enum context_state {
	pending,
	ready
};

template<typename...Ret>
struct context;

template<>
struct context<> {
	void* regs[8];
	char* stack_;
	std::size_t stack_size_;
	bool start = false;
	context_state state_ = context_state::pending;
	int id;
	~context() {
		if (stack_ != nullptr) {
			free(stack_);
			stack_ = nullptr;
			stack_size_ = 0;
		}
	}
};
template<typename Ret>
struct context<Ret> : context<> {
	std::function<Ret()> func_;
};
enum regs_type {
	ret,
	esp,
	ebp,
	ebx,
	ecx,
	edx,
	edi,
	esi
};
struct ctx_params_t {
	void* s0;
	void* s1;
};
extern "C" {
	void swap_context(void*, void*);
}
class coco {
public:
	coco() {
		auto ctx = new context<void>();
		ctx->id = 0;
		context_stack_.push_back(ctx);
		ctx_index_++;
	}
public:
	template<typename Function, typename...Args>
	static auto co_create(Function&& function, Args&&...args)->context<decltype(function(std::forward<Args>(args)...))>* {
		using Ret = decltype(function(std::forward<Args>(args)...));
		std::function<Ret()> func = std::bind(std::forward<Function>(function), std::forward<Args>(args)...);
		auto ctx_ = new context<Ret>();
		ctx_->func_ = func;
		return ctx_;
	}
	template<typename Ret>
	static void co_resume(context<Ret>* ctx) {
		auto& this_ = instance();
		auto current = this_.context_stack_[this_.ctx_index_ - 1];
		this_.context_stack_.push_back(ctx);
		this_.ctx_index_++;
		if (ctx->start == false) {
			co_make(ctx);
			ctx->start = true;
		}
		swap_context(current, ctx);
	}
	static void co_yiled() {
		auto& this_ = instance();
		auto last = this_.context_stack_[this_.ctx_index_ - 2];
		auto current = this_.context_stack_[this_.ctx_index_ - 1];
		this_.context_stack_.pop_back();
		this_.ctx_index_--;
		swap_context(current, last);
	}
	static context<>* add_task() {
		auto& this_ = instance();
		auto current = this_.context_stack_[this_.ctx_index_ - 1];
		this_.task_list_.push_back(current);
		return current;
	}
	static void resume(context<>* ctx) {
		ctx->state_ = context_state::ready;
	}

	static void co_wait() {
		auto& this_ = coco::instance();
		auto& list = this_.task_list_;
		this_.wait_ctx_ = coco::co_create([list, wait_ctx_ = this_.wait_ctx_]() mutable{
			while (true) {
				for (auto iter = list.begin(); iter != list.end();) {
					if ((*iter)->state_ == context_state::ready) {
						auto ctx = static_cast<context<void>*>(*iter);
						coco::co_resume(ctx);
						iter = list.erase(iter);
						continue;
					}
					iter++;
				}
				if (list.size() == 0) {
					return;
				}
			}
		});
		this_.wait_ctx_->id = 102;
		coco::co_resume(this_.wait_ctx_);
	}

private:
	template<typename Ret>
	static void co_end() {
		static context<> tmp_ctx;
		auto& this_ = instance();
		auto last = this_.context_stack_[this_.ctx_index_ - 2];
		auto current = this_.context_stack_[this_.ctx_index_ - 1];
		this_.context_stack_.pop_back();
		this_.ctx_index_--;
		this_.unused_context_.push_back(std::shared_ptr<context<Ret>>(static_cast<context<Ret>*>(current)));
		swap_context(&tmp_ctx, last);
	}
private:
	template<typename Ret>
	static void co_start(context<Ret>* curr, context<>* pend) {
		if (curr->func_) {
			(curr->func_)();
			coco::co_end<Ret>();
		}
	}
	template<typename T>
	static void co_make(context<T>* ctx) {
		auto this_ = instance();
		memset(ctx->regs, 0, sizeof(ctx->regs));
		ctx->stack_size_ = 128 * 1024;
		ctx->stack_ = (char*)malloc(ctx->stack_size_);
		memset(ctx->stack_, 0, ctx->stack_size_);
		ctx->regs[regs_type::ret] = (void*)&coco::co_start<T>;
		char* sp = ctx->stack_ + ctx->stack_size_;
		sp = (char*)((unsigned long)sp & -16L);
		ctx_params_t* params = (ctx_params_t*)(sp - sizeof(ctx_params_t));
		auto current = this_.context_stack_[this_.ctx_index_ - 2];
		params->s0 = ctx;
		params->s1 = current;
		char* esp = sp - sizeof(ctx_params_t) - sizeof(void*);
		ctx->regs[regs_type::esp] = (void*)esp;
	}
private:
	static coco& instance() {
		static coco coco_{};
		return coco_;
	}
private:
	std::size_t ctx_index_ = 0;
	std::vector<context<>*> context_stack_;
	std::vector<std::shared_ptr<context<>>> unused_context_;
	std::vector<context<>*> task_list_;
	context<void>* wait_ctx_;
};


void func1() {
	std::cout << "in func1" << std::endl;
	coco::co_yiled();
	std::cout << "out func1" << std::endl;
}
struct ctx {
	void* regs[8];
	char* stack_;
	std::size_t stack_size_;
};



//espA4,  //第一个参数
//espA8,   //第二个参数,

void func0() {
	int v = 2048;
	std::cout << "in func0 " << v << std::endl;
	auto current = coco::add_task();
	std::thread t0([current]() {
		std::this_thread::sleep_for(std::chrono::seconds(3));
		coco::resume(current);
	});
	t0.detach();
	coco::co_yiled();
	std::cout << "I am back " << v << std::endl;
}
int main() {
	auto ctx = coco::co_create(&func0);
	auto ctx1 = coco::co_create(&func1);
	ctx->id = 1;
	coco::co_resume(ctx);
	//coco::co_resume(ctx1);
	std::cout << "abc" << std::endl;
	//coco::co_resume(ctx);
	//coco::co_resume(ctx1);
	coco::co_wait();
	//std::this_thread::sleep_for(std::chrono::seconds(4));
	//coco::co_resume(ctx);
	std::getchar();
}
