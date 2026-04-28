#pragma once


#include "Modules/ModuleInterface.h"


struct FIPv4Endpoint;

class FLongboyModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual TUniquePtr<struct FLongboyClient> CreateClient(const FIPv4Endpoint& RemoteEndpoint);

private:
	TUniquePtr<struct FQuicFactory> QuicFactory;
};
