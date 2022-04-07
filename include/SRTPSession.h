#ifndef SRTPSESSION_H
#define SRTPSESSION_H
#include <srtp2/srtp.h>
#include <vector>
#include "config.h"

class SRTPSession
{
public:
	enum Status {
		OK		= srtp_err_status_ok           ,  /**< nothing to report                       */
		Fail		= srtp_err_status_fail         ,  /**< unspecified failure                     */
		BadParam	= srtp_err_status_bad_param    ,  /**< unsupported parameter                   */
		AllocFail	= srtp_err_status_alloc_fail   ,  /**< couldn't allocate memory                */
		DeallocFail	= srtp_err_status_dealloc_fail ,  /**< couldn't deallocate properly            */
		InitFail	= srtp_err_status_init_fail    ,  /**< couldn't initialize                     */
		Terminus	= srtp_err_status_terminus     ,  /**< can't process as much data as requested */
		AuthFail	= srtp_err_status_auth_fail    ,  /**< authentication failure                  */
		CipherFail	= srtp_err_status_cipher_fail  ,  /**< cipher failure                          */
		ReplayFail	= srtp_err_status_replay_fail  ,  /**< replay check failed (bad index)         */
		ReplayOld	= srtp_err_status_replay_old   , /**< replay check failed (index too old)     */
		AlgoFail	= srtp_err_status_algo_fail    , /**< algorithm failed test routine           */
		NoSuchOp	= srtp_err_status_no_such_op   , /**< unsupported operation                   */
		NoCtx		= srtp_err_status_no_ctx       , /**< no appropriate context found            */
		CantCheck	= srtp_err_status_cant_check   , /**< unable to perform desired validation    */
		KeyExpired	= srtp_err_status_key_expired  , /**< can't use key any more                  */
		SocketErr	= srtp_err_status_socket_err   , /**< error in use of socket                  */
		SignalErr	= srtp_err_status_signal_err   , /**< error in use POSIX signals              */
		NonceBad	= srtp_err_status_nonce_bad    , /**< nonce check failed                      */
		ReadFail	= srtp_err_status_read_fail    , /**< couldn't read data                      */
		WriteFail	= srtp_err_status_write_fail   , /**< couldn't write data                     */
		ParseErr	= srtp_err_status_parse_err    , /**< error parsing data                      */
		EncodeErr	= srtp_err_status_encode_err   , /**< error encoding data                     */
		SemaphoreErr	= srtp_err_status_semaphore_err,/**< error while using semaphores            */
		PFKeyErr	= srtp_err_status_pfkey_err     /**< error while using pfkey                 */
      };
public:
	SRTPSession() = default;
	~SRTPSession();
	
	bool Setup(const char* suite,const uint8_t* key,const size_t len);
	void Reset();
	
	void AddStream(uint32_t ssrc);
	void RemoveStream(uint32_t ssrc);
	
	size_t ProtectRTP(uint8_t* data, size_t size);
	size_t ProtectRTCP(uint8_t* data, size_t size);
	size_t UnprotectRTP(uint8_t* data, size_t size);
	size_t UnprotectRTCP(uint8_t* data, size_t size);
	
	bool IsSetup() const { return srtp; }
	const char* GetLastError() const
	{
		switch(err)
		{
			case OK			: return "OK";
			case Fail		: return "Fail";
			case BadParam		: return "BadParam";
			case AllocFail		: return "AllocFail";
			case DeallocFail	: return "DeallocFail";
			case InitFail		: return "InitFail";
			case Terminus		: return "Terminus";
			case AuthFail		: return "AuthFail";
			case CipherFail		: return "CipherFail";
			case ReplayFail		: return "ReplayFail";
			case ReplayOld		: return "ReplayOld";
			case AlgoFail		: return "AlgoFail";
			case NoSuchOp		: return "NoSuchOp";
			case NoCtx		: return "NoCtx";
			case CantCheck		: return "CantCheck";
			case KeyExpired		: return "KeyExpired";
			case SocketErr		: return "SocketErr";
			case SignalErr		: return "SignalErr";
			case NonceBad		: return "NonceBad";
			case ReadFail		: return "ReadFail";
			case WriteFail		: return "WriteFail";
			case ParseErr		: return "ParseErr";
			case EncodeErr		: return "EncodeErr";
			case SemaphoreErr	: return "SemaphoreErr";
			case PFKeyErr		: return "PFKeyErr";
		}
		return "Uknown";
	}
	Status GetLastStatus() const { return err; }
private:
	srtp_t srtp = nullptr;
	Status err = Status::OK;
	srtp_policy_t policy = {};
	std::vector<uint8_t> key;
	std::vector<uint32_t> pending;
};

#endif /* SRTPSESSION_H */

