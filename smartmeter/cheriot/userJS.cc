#define CHERIOT_NO_AMBIENT_MALLOC
#include "common.hh"
#include "default-javascript.h"
#include "microvium-ffi.hh"
#include <locks.hh>

DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(JavaScriptMallocCapability, 9 * 1024);
#define JAVASCRIPT_MALLOC STATIC_SEALED_VALUE(JavaScriptMallocCapability)

using Debug = ConditionalDebug<true, "JavaScript sandbox compartment">;

using CHERI::Capability;

std::unique_ptr<mvm_VM, MVMDeleter> vm;
const uint8_t                      *claimed_bytecode;
;

FlagLock lock;

/*
 * In monolith builds, this is _the_ compartment error handler!
 */
enum ErrorRecoveryBehaviour
compartment_error_handler(struct ErrorState *frame, size_t mcause, size_t mtval)
{
	Debug::log("Crash detected in JavaScript compartment.  PCC: {}",
	           frame->pcc);
	vm.reset(nullptr);
	lock.unlock();
	return ErrorRecoveryBehaviour::ForceUnwind;
}

static void cleanup()
{
	vm.reset(nullptr);
	claimed_bytecode = nullptr;
	heap_free_all(JAVASCRIPT_MALLOC);
}

int user_javascript_run(/* XXX */)
{
	LockGuard g{lock};
	if (!vm)
	{
		Debug::log("No dynamic JavaScript bytecode provided, loading default "
		           "JavaScript");
		mvm_VM *rawVm;
		int     err = mvm_restore(
          &rawVm,            /* Out pointer to the VM */
          hello_mvm_bc,      /* Bytecode data */
          hello_mvm_bc_len,  /* Bytecode length */
          JAVASCRIPT_MALLOC, /* Capability used to allocate memory */
          ::resolve_import); /* Callback used to resolve FFI imports */
		// If this is not valid bytecode, give up.
		Debug::Assert(
		  err == MVM_E_SUCCESS, "Failed to parse bytecode: {}", err);
		vm.reset(rawVm);
	}
	mvm_Value run;
	if (mvm_resolveExports(vm.get(), &ExportRun, &run, 1) == MVM_E_SUCCESS)
	{
		// Allocate the space for the VM capability registers on the stack and
		// record its location.
		// **Note**: This must be on the same stack and in same compartment as
		// the JavaScript interpreter, so that the callbacks can re-derive it
		// from csp.
		AttackerRegisterState state;

		state_set(&state);

		// Set a limit of bytecodes to execute, to prevent infinite loops.
		mvm_stopAfterNInstructions(vm.get(), 20000);
		// Call the function:
		int err = mvm_call(vm.get(), run, nullptr, nullptr, 0);
		if (err != MVM_E_SUCCESS)
		{
			Debug::log("Failed to call run function: {}", err);
			cleanup();
			return -EINVAL;
		}
		return 0;
	}
	return -EINVAL;
}

int user_javascript_load(const uint8_t *bytecode, size_t size)
{
	LockGuard g{lock};
	////////////////////////////////////////////////////////////////////////
	// We've now read the bytecode into a buffer.  Spin up the JavaScript
	// VM to execute it.
	////////////////////////////////////////////////////////////////////////

	Debug::log("{} bytes of heap quota available",
	           heap_quota_remaining(JAVASCRIPT_MALLOC));
	heap_free(JAVASCRIPT_MALLOC, const_cast<uint8_t *>(claimed_bytecode));
	heap_claim(JAVASCRIPT_MALLOC, const_cast<uint8_t *>(bytecode));
	claimed_bytecode = bytecode;

	// MMIO_CAPABILITY(GPIO, gpio_led0)->enable_all();

	mvm_TeError err;
	// Create a Microvium VM from the bytecode.
	{
		mvm_VM *rawVm;
		err = mvm_restore(
		  &rawVm,                                  /* Out pointer to the VM */
		  const_cast<uint8_t *>(claimed_bytecode), /* Bytecode data */
		  size,                                    /* Bytecode length */
		  JAVASCRIPT_MALLOC, /* Capability used to allocate memory */
		  ::resolve_import); /* Callback used to resolve FFI imports */
		// If this is not valid bytecode, give up.
		Debug::Assert(
		  err == MVM_E_SUCCESS, "Failed to parse bytecode: {}", err);
		vm.reset(rawVm);
	}

	return 0;
}
