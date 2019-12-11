#pragma once
#include <iostream>
#include <vector>
#include <functional>
#include <cstring>
#include <thread>
extern "C" {
	//void swap_context(void*, void*);
#ifdef _WIN32
	void swap_context(void*, void*) {
#ifdef _WIN64 

#else
		__asm {
			leave
			mov eax, [esp + 4]  //第一个参数
			mov[eax + 8], ebp  //保存ebp
			mov[eax + 12], ebx
			mov[eax + 16], ecx
			mov[eax + 20], edx
			mov[eax + 24], edi
			mov[eax + 28], esi
			xor ecx, ecx
			mov ecx, [esp]  //返回地址
			mov[eax], ecx
			xor ecx, ecx
			lea ecx, [esp + 4]
			mov[eax + 4], ecx

			xor eax, eax
			mov eax, [esp + 8] //第二个参数
			mov esp, [eax + 4]
			mov ebp, [eax + 8]
			mov ebx, [eax + 12]
			mov ecx, [eax + 16]
			mov edx, [eax + 20]
			mov edi, [eax + 24]
			mov esi, [eax + 28]
			mov eax, [eax]
			jmp eax
		}
#endif
	}
#else
	void swap_context(void*, void*) {
		asm(
			"leave\r\n"
			"leaq 8(%rsp),%rax\r\n"
			"movq %rax,8(%rdi)\r\n"
			"movq -8(%rax),%rax\r\n"
			"movq %rax,(%rdi)\r\n"
			"movq %rbp,16(%rdi)\r\n"
			"movq %rbx,24(%rdi)\r\n"
			"movq %rcx,32(%rdi)\r\n"
			"movq %rdx,40(%rdi)\r\n"
			"movq %rdi,48(%rdi)\r\n"
			"movq %rsi,56(%rdi)\r\n"
			"movq %r8,64(%rdi)\r\n"
			"movq %r9,72(%rdi)\r\n"
			"movq %r12,80(%rdi)\r\n"
			"movq %r13,88(%rdi)\r\n"
			"movq %r14,96(%rdi)\r\n"
			"movq %r15,104(%rdi)\r\n"

			"movq 8(%rsi),%rsp\r\n"
			"movq 16(%rsi),%rbp\r\n"
			"movq 24(%rsi),%rbx\r\n"
			"movq 32(%rsi),%rcx\r\n"
			"movq 40(%rsi),%rdx\r\n"
			"movq 48(%rsi),%rdi\r\n"
			"movq 64(%rsi),%r8\r\n"
			"movq 72(%rsi),%r9\r\n"
			"movq 80(%rsi),%r12\r\n"
			"movq 88(%rsi),%r13\r\n"
			"movq 96(%rsi),%r14\r\n"
			"movq 104(%rsi),%r15\r\n"
			"movq (%rsi),%rax\r\n"
			"movq 56(%rsi),%rsi\r\n"
			"jmp *%rax\r\n"
		);
    }
#endif
}
enum context_state {
	pending,
	ready
};
template<typename...Ret>
struct context;

template<>
struct context<> {
#ifdef _WIN32
	void* regs[8];
#else // _WIN32
#ifdef  __x86_64__
	void* regs[13];
#endif //  __x86_64__
#endif
	char* stack_ = nullptr;
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
class coco {
private:

#ifdef _WIN32
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
#else
	enum regs_type {
		ret,
		rsp,
		rbp,
		rbx,
		rcx,
		rdx,
		rdi,
		rsi,
		r8,
		r9,
		r12,
		r13,
		r14,
		r15
};
#endif
	struct ctx_params_t {
		void* s0;
		void* s1;
	};
public:
	coco() {
		auto ctx = new context<void>();
		ctx->id = 0;
		context_stack_.push_back(ctx);
		ctx_index_++;
	}
public:
	template<typename Function, typename...Args>
	static auto co_create(Function&& function, Args&& ...args)->context<decltype(function(std::forward<Args>(args)...))>* {
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
		std::shared_ptr<context<Ret>> shared(static_cast<context<Ret>*>(current));
		this_.unused_context_.push_back(shared);
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
#ifdef  _WIN32
	template<typename T>
	static void co_make(context<T>* ctx) {
		auto this_ = instance();
		memset(ctx->regs, 0, sizeof(ctx->regs));
		ctx->stack_size_ = 128 * 1024;
		ctx->stack_ = (char*)malloc(ctx->stack_size_);
		memset(ctx->stack_, 0, ctx->stack_size_);
		ctx->regs[regs_type::ret] = (void*)& coco::co_start<T>;
		char* sp = ctx->stack_ + ctx->stack_size_;
		sp = (char*)((unsigned long)sp & -16L);
		ctx_params_t* params = (ctx_params_t*)(sp - sizeof(ctx_params_t));
		auto current = this_.context_stack_[this_.ctx_index_ - 2];
		params->s0 = ctx;
		params->s1 = current;
		char* esp = sp - sizeof(ctx_params_t) - sizeof(void*);
		ctx->regs[regs_type::esp] = (void*)esp;
	}
#else //  _WIN32
	template<typename T>
	static void co_make(context<T>* ctx) {
		auto this_ = instance();
		memset(ctx->regs, 0, sizeof(ctx->regs));
		ctx->stack_size_ = 128 * 1024;
		ctx->stack_ = (char*)malloc(ctx->stack_size_);
		memset(ctx->stack_, 0, ctx->stack_size_);
		ctx->regs[regs_type::ret] = (void*)& coco::co_start<T>;
		char* sp = ctx->stack_ + ctx->stack_size_;
		sp = (char*)((unsigned long)sp & -16LL);
		ctx->regs[regs_type::rsp] = (void*)(sp - sizeof(void*));
		auto current = this_.context_stack_[this_.ctx_index_ - 2];
		ctx->regs[regs_type::rdi] = ctx;
		ctx->regs[regs_type::rsi] = current;
	}
#endif


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