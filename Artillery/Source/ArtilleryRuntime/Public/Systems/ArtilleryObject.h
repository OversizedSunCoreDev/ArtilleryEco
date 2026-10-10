// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

struct FArtilleryDataBuffer;

// An artillery object should be able to safely rstore and restore itself so that we can support storing+restoring for rollbacking it
// Mainly intended for "predicted" objects that can run outside of verified frames but there are reasons you could desire to rollback the entire game state (debugging, saving/loading etc)
class ARTILLERYRUNTIME_API IArtilleryObject
{
public:
	virtual ~IArtilleryObject() = default;
	
	void Store();
	void Load();
	
	// Optional overload to indicate you want us to store a scriptstruct in each state ahead of time
	virtual UScriptStruct* GetArtilleryStateType() { return nullptr; };
protected:
	// How to store and load data is up to you. The generic way will be to add a new instanced struct to GenericData
	virtual void StoreArtilleryState(FArtilleryDataBuffer& State) = 0;
	virtual void LoadArtilleryState(const FArtilleryDataBuffer& State) = 0;
	

};
