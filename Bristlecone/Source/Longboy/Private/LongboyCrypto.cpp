#include "LongboyCrypto.h"

FLongboyCrypto::FLongboyCrypto(const uint64& Key)
	: HeaderCipher({
		reinterpret_cast<const uint8*>(&Key)[0],
		reinterpret_cast<const uint8*>(&Key)[1],
		reinterpret_cast<const uint8*>(&Key)[2],
		reinterpret_cast<const uint8*>(&Key)[3],
		reinterpret_cast<const uint8*>(&Key)[4],
		reinterpret_cast<const uint8*>(&Key)[5],
		reinterpret_cast<const uint8*>(&Key)[6],
		reinterpret_cast<const uint8*>(&Key)[7],
	})
	, BodyCipher({
		reinterpret_cast<const uint8*>(&Key)[0],
		reinterpret_cast<const uint8*>(&Key)[1],
		reinterpret_cast<const uint8*>(&Key)[2],
		reinterpret_cast<const uint8*>(&Key)[3],
		reinterpret_cast<const uint8*>(&Key)[4],
		reinterpret_cast<const uint8*>(&Key)[5],
		reinterpret_cast<const uint8*>(&Key)[6],
		reinterpret_cast<const uint8*>(&Key)[7],
	})
{
}

void FLongboyCrypto::EncryptHeader(const uint32* Header, uint32* OutHeader)
{
	std::array<uint8_t, 4Ui64> Block;
	FMemory::Memcpy(std::data(Block), Header, 4);
	HeaderCipher.encrypt(Block);
	FMemory::Memcpy(OutHeader, std::data(Block), 4);
}

void FLongboyCrypto::DecryptHeader(const uint32* Header, uint32* OutHeader)
{
	std::array<uint8_t, 4Ui64> Block;
	FMemory::Memcpy(std::data(Block), Header, 4);
	HeaderCipher.decrypt(Block);
	FMemory::Memcpy(OutHeader, std::data(Block), 4);
}

void FLongboyCrypto::EncryptBody(const TArray<uint8>& Plaintext, TArray<uint8>& OutCiphertext)
{
	const uint8* Memory = Plaintext.GetData();
	const auto ByteSize = Plaintext.NumBytes();

	//get total blocks to size output array
	const auto TotalBlocks = (ByteSize + 7) / 8; // Round up to nearest block
	OutCiphertext.SetNum(TotalBlocks * 8); // Ensure output is large enough for all blocks
	
	std::array<uint8_t, 8Ui64> Block;
	for (int i = 0; i < ByteSize; i += 8)
	{
		FMemory::Memcpy(std::data(Block), Memory + i, 8);
		BodyCipher.encrypt(Block);
		FMemory::Memcpy(OutCiphertext.GetData() + i, std::data(Block), 8);
	}
}

void FLongboyCrypto::DecryptBody(const TArray<uint8>& Ciphertext, TArray<uint8>& OutPlaintext)
{
	const uint8* Memory = Ciphertext.GetData();
	const auto ByteSize = Ciphertext.NumBytes();

	//get total blocks to size output array
	const auto TotalBlocks = (ByteSize + 7) / 8; // Round up to nearest block
	OutPlaintext.SetNum(TotalBlocks * 8); // Ensure output is large enough for all blocks

	std::array<uint8_t, 8Ui64> Block;
	for (int i = 0; i < ByteSize; i += 8)
	{
		FMemory::Memcpy(std::data(Block), Memory + i, 8);
		BodyCipher.encrypt(Block);
		FMemory::Memcpy(OutPlaintext.GetData() + i, std::data(Block), 8);
	}

}

void FLongboyCrypto::EncryptBody(const uint8* Plaintext, uint8* OutCiphertext, SIZE_T NumBytes)
{
	const auto TotalBlocks = (NumBytes + 7) / 8; // Round up to nearest block
	std::array<uint8_t, 8Ui64> Block;
	for (int i = 0; i < NumBytes; i += 8)
	{
		FMemory::Memcpy(std::data(Block), Plaintext + i, 8);
		BodyCipher.encrypt(Block);
		FMemory::Memcpy(OutCiphertext + i, std::data(Block), 8);
	}
}

void FLongboyCrypto::DecryptBody(const uint8* Ciphertext, uint8* OutPlaintext, SIZE_T NumBytes)
{
	const auto TotalBlocks = (NumBytes + 7) / 8; // Round up to nearest block
	std::array<uint8_t, 8Ui64> Block;
	for (int i = 0; i < NumBytes; i += 8)
	{
		FMemory::Memcpy(std::data(Block), Ciphertext + i, 8);
		BodyCipher.decrypt(Block);
		FMemory::Memcpy(OutPlaintext + i, std::data(Block), 8);
	}
}