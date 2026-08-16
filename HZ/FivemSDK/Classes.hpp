#pragma once

#include "Offsets.hpp"
#include "GTADefines.hpp"

#include "../Math/Vectors/Vector2D.hpp"
#include "../Math/Vectors/Vector3D.hpp"
#include "../Math/Vectors/Vector4D.hpp"
#include "../Utils/Memory.hpp"

#include <cstring>

class CPed;

namespace Cheat
{
	class CWeaponInfo
	{
	public:
		std::string GetWeaponName()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadProcessMemoryString(FrameWork::Memory::ReadMemory<uint64_t>(this + 0x05F0));
		}
	};

	class CWeaponManager
	{
	public:
		CWeaponInfo* GetWeaponInfo()
		{
			if (!this)
				return 0;

			return (CWeaponInfo*)FrameWork::Memory::ReadMemory<uint64_t>(this + 0x20);
		}
	};

	class CVehicle
	{
	public:
		uint64_t GetModelInfo()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<uint64_t>(this + 0x20);
		}

		void FuckVehicleEngine()
		{
			if (!this) return;

			Vector3D pos = GetCoordinate();

			FrameWork::Memory::WriteMemory<float>((uintptr_t)this + 0x844, -4000.f);
			this->SetHealth(-4000.f);
		}

		void FixEngine()
		{
			if (!this) return;

			Vector3D pos = GetCoordinate();

			FrameWork::Memory::WriteMemory<float>(this + 0x844, 1000.f);
			SetHealth(1000.f);
		}

		uint64_t GetNavigation()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<uint64_t>(this + 0x30);
		}

		void SetLockState(eCarLockState NewState)
		{
			if (!this)
				return;

			FrameWork::Memory::WriteMemory(this + Offsets::DoorLock, (unsigned int)NewState);
		}
		void SetFixed()
		{
			if (!this)
				return;

			FrameWork::Memory::WriteMemory<float>(this + Offsets::VehicleEngineHealth, 1000.f);
			FrameWork::Memory::WriteMemory<float>(this + 0x280, 1000.f); 
		}
		void Fix() {
			if (!this) { return; }
			float Value = 1000.0f;
			FrameWork::Memory::WriteMemory<float>(reinterpret_cast<uintptr_t>(this) + 0x970, Value);
		}
		bool GetLockState()
		{
			if (!this)
				return false;

			return FrameWork::Memory::ReadMemory<eCarLockState>(this + Offsets::DoorLock);
		}

		void SetHealth(float NewHealth)
		{
			if (!this)
				return;

			FrameWork::Memory::WriteMemory(this + 0x280, NewHealth);
		}

		Vector3D GetCoordinate()
		{
			if (!this)
				return Vector3D{ 0,0,0 };

			return FrameWork::Memory::ReadMemory<Vector3D>(this + 0x90);
		}

		static uint8_t EncodePlateChar(char c)
		{
			if (c == ' ')
				return 0x3F;

			if (c >= 'a' && c <= 'z')
				c = static_cast<char>(c - ('a' - 'A'));

			if (c >= '0' && c <= '9')
				return static_cast<uint8_t>(c - '0');

			if (c >= 'A' && c <= 'Z')
				return static_cast<uint8_t>(c - 'A' + 0x0A);

			return 0x3F;
		}

		static bool IsValidRemotePointer(uint64_t address)
		{
			if (address < 0x10000 || address > 0x00007FFFFFFFFFFF)
				return false;

			return FrameWork::Memory::ReadBytes(address, 1).size() == 1;
		}

		static char DecodePlateChar(uint8_t value)
		{
			if (value <= 0x09)
				return static_cast<char>('0' + value);

			if (value >= 0x0A && value <= 0x23)
				return static_cast<char>('A' + (value - 0x0A));

			if (value == 0x3F || value == 0x63)
				return ' ';

			return '\0';
		}

		static bool BufferLooksLikeAsciiPlate(const char* data, bool allowBlank = false)
		{
			int alnum = 0;
			bool sawNonSpace = false;

			for (int i = 0; i < 8; ++i)
			{
				const unsigned char c = static_cast<unsigned char>(data[i]);
				if (c == '\0')
					break;

				if (c == ' ')
					continue;

				sawNonSpace = true;

				if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
				{
					++alnum;
					continue;
				}

				return false;
			}

			if (allowBlank && !sawNonSpace)
				return true;

			return alnum >= 2;
		}

		static void NormalizePlateBuffer(const char* input, char out[8])
		{
			for (int i = 0; i < 8; ++i)
				out[i] = ' ';

			if (!input)
				return;

			for (int i = 0; i < 8 && input[i] != '\0'; ++i)
			{
				char c = input[i];
				if (c >= 'a' && c <= 'z')
					c = static_cast<char>(c - ('a' - 'A'));

				out[i] = c;
			}
		}

		static void DecodeEncodedPlate(const uint8_t* encoded, char out[8])
		{
			for (int i = 0; i < 8; ++i)
			{
				const char c = DecodePlateChar(encoded[i]);
				out[i] = (c == '\0') ? ' ' : c;
			}
		}

		static bool IsStrictEncodedPlateByte(uint8_t value)
		{
			return value <= 0x23 || value == 0x3F || value == 0x63;
		}

		static bool BytesAreStrictEncodedPlate(const uint8_t* data)
		{
			for (int i = 0; i < 8; ++i)
			{
				if (!IsStrictEncodedPlateByte(data[i]))
					return false;
			}

			char decoded[8] = {};
			DecodeEncodedPlate(data, decoded);
			return BufferLooksLikeAsciiPlate(decoded, false);
		}

		static bool PlatesMatchNormalized(const char* a, const char* b)
		{
			char normalizedA[8] = {};
			char normalizedB[8] = {};
			NormalizePlateBuffer(a, normalizedA);
			NormalizePlateBuffer(b, normalizedB);
			return std::memcmp(normalizedA, normalizedB, 8) == 0;
		}

		static bool ReadEncodedPlateAt(uint64_t address, uint8_t encoded[8], char decoded[8])
		{
			if (!address)
				return false;

			auto bytes = FrameWork::Memory::ReadBytes(address, 8);
			if (bytes.size() != 8 || !BytesAreStrictEncodedPlate(bytes.data()))
				return false;

			std::memcpy(encoded, bytes.data(), 8);
			DecodeEncodedPlate(encoded, decoded);
			return BufferLooksLikeAsciiPlate(decoded, false);
		}

		static void SanitizePlateBuffer(char buf[8])
		{
			for (int i = 0; i < 8; ++i)
			{
				if (buf[i] == '\0')
				{
					for (int j = i; j < 8; ++j)
						buf[j] = ' ';
					break;
				}
			}
		}

		static void EncodePlateBuffer(const char* asciiPlate, uint8_t out[8])
		{
			for (int i = 0; i < 8; ++i)
				out[i] = EncodePlateChar(asciiPlate ? asciiPlate[i] : ' ');
		}

		static bool ReadAsciiPlateAt(uint64_t address, char out[8])
		{
			if (!address)
				return false;

			auto bytes = FrameWork::Memory::ReadBytes(address, 8);
			if (bytes.size() != 8)
				return false;

			std::memcpy(out, bytes.data(), 8);
			SanitizePlateBuffer(out);
			return BufferLooksLikeAsciiPlate(out, false);
		}

		static bool ReadAsciiPlateViaStringPointer(uint64_t address, char out[8])
		{
			if (!address)
				return false;

			const uint64_t pointer = FrameWork::Memory::ReadMemory<uint64_t>(address);
			if (!IsValidRemotePointer(pointer))
				return false;

			const std::string plateText = FrameWork::Memory::ReadProcessMemoryString(pointer, 16);
			if (plateText.empty() || plateText.size() > 8)
				return false;

			NormalizePlateBuffer(plateText.c_str(), out);
			return BufferLooksLikeAsciiPlate(out, false);
		}

		static bool ReadUtf16PlateAt(uint64_t address, char out[8])
		{
			if (!address)
				return false;

			auto bytes = FrameWork::Memory::ReadBytes(address, 16);
			if (bytes.size() != 16)
				return false;

			for (int i = 0; i < 8; ++i)
			{
				const wchar_t wc = *reinterpret_cast<const wchar_t*>(bytes.data() + (i * sizeof(wchar_t)));
				if (wc == L'\0')
				{
					for (int j = i; j < 8; ++j)
						out[j] = ' ';
					break;
				}

				if (wc > 127)
					return false;

				out[i] = static_cast<char>(wc);
			}

			return BufferLooksLikeAsciiPlate(out, false);
		}

		static bool ReadCurrentPlateFromShader(uint64_t shaderEffect, char decodedPlate[8])
		{
			if (!shaderEffect)
				return false;

			const uint64_t fieldOffsets[] = {
				Offsets::VehiclePlateShader,
				0x138,
				0x130,
			};

			for (uint64_t fieldOffset : fieldOffsets)
			{
				if (!fieldOffset)
					continue;

				const uint64_t fieldAddress = shaderEffect + fieldOffset;
				const uint64_t fieldValue = FrameWork::Memory::ReadMemory<uint64_t>(fieldAddress);

				uint8_t fieldBytes[8] = {};
				std::memcpy(fieldBytes, &fieldValue, 8);

				uint8_t encoded[8] = {};
				char decoded[8] = {};

				if (ReadEncodedPlateAt(fieldAddress, encoded, decoded))
				{
					std::memcpy(decodedPlate, decoded, 8);
					return true;
				}

				if (IsValidRemotePointer(fieldValue) && !BytesAreStrictEncodedPlate(fieldBytes))
				{
					if (ReadEncodedPlateAt(fieldValue, encoded, decoded))
					{
						std::memcpy(decodedPlate, decoded, 8);
						return true;
					}
				}
			}

			return false;
		}

		struct PlateWriteTarget
		{
			uint64_t address = 0;
			bool utf16 = false;
			bool encoded = false;
		};

		static uint64_t ResolveVehicleShaderEffect(uintptr_t vehicle, char referencePlate[8])
		{
			if (!vehicle)
				return 0;

			const uint64_t shaderOffsets[] = { 0x18, 0x20, 0x28, 0x30 };
			const uint64_t drawHandler = FrameWork::Memory::ReadMemory<uint64_t>(vehicle + 0x48);
			if (!IsValidRemotePointer(drawHandler))
				return 0;

			for (uint64_t shaderOffset : shaderOffsets)
			{
				const uint64_t shaderEffect = FrameWork::Memory::ReadMemory<uint64_t>(drawHandler + shaderOffset);
				if (!IsValidRemotePointer(shaderEffect))
					continue;

				if (ReadCurrentPlateFromShader(shaderEffect, referencePlate))
					return shaderEffect;
			}

			return 0;
		}

		static uint64_t FindEncodedPlateWriteAddress(uint64_t shaderEffect)
		{
			if (!shaderEffect)
				return 0;

			const uint64_t fieldOffsets[] = {
				Offsets::VehiclePlateShader,
				0x138,
				0x130,
			};

			for (uint64_t fieldOffset : fieldOffsets)
			{
				if (!fieldOffset)
					continue;

				const uint64_t fieldAddress = shaderEffect + fieldOffset;
				const uint64_t fieldValue = FrameWork::Memory::ReadMemory<uint64_t>(fieldAddress);

				uint8_t encoded[8] = {};
				char decoded[8] = {};

				if (ReadEncodedPlateAt(fieldAddress, encoded, decoded))
					return fieldAddress;

				if (IsValidRemotePointer(fieldValue) && ReadEncodedPlateAt(fieldValue, encoded, decoded))
					return fieldValue;
			}

			return 0;
		}

		static uint64_t ScanVehicleForEncodedPattern(uintptr_t vehicle, const uint8_t* encoded)
		{
			if (!vehicle || !encoded)
				return 0;

			auto matchesEncoded = [encoded](const uint8_t* candidate) -> bool
			{
				if (std::memcmp(candidate, encoded, 8) == 0)
					return true;

				if (std::memcmp(candidate, encoded, 7) == 0)
				{
					const uint8_t last = candidate[7];
					return last == encoded[7] || last == 0x3F || last == 0x63;
				}

				return false;
			};

			for (uint64_t offset = 0x400; offset <= 0x2000; ++offset)
			{
				auto bytes = FrameWork::Memory::ReadBytes(vehicle + offset, 8);
				if (bytes.size() == 8 && matchesEncoded(bytes.data()))
					return vehicle + offset;
			}

			const uint64_t extension = FrameWork::Memory::ReadMemory<uint64_t>(vehicle + 0x1398);
			if (IsValidRemotePointer(extension))
			{
				for (uint64_t offset = 0x0; offset <= 0x200; ++offset)
				{
					auto bytes = FrameWork::Memory::ReadBytes(extension + offset, 8);
					if (bytes.size() == 8 && matchesEncoded(bytes.data()))
						return extension + offset;
				}
			}

			return 0;
		}

		static bool TryMatchPlateAt(uint64_t address, const char* expectedPlate, char out[8], bool& utf16, bool& encoded)
		{
			encoded = false;

			if (ReadAsciiPlateAt(address, out))
			{
				if (PlatesMatchNormalized(out, expectedPlate))
				{
					utf16 = false;
					return true;
				}
			}

			if (ReadUtf16PlateAt(address, out))
			{
				if (PlatesMatchNormalized(out, expectedPlate))
				{
					utf16 = true;
					return true;
				}
			}

			if (ReadAsciiPlateViaStringPointer(address, out) && PlatesMatchNormalized(out, expectedPlate))
			{
				utf16 = false;
				return true;
			}

			uint8_t encodedBytes[8] = {};
			if (ReadEncodedPlateAt(address, encodedBytes, out) && PlatesMatchNormalized(out, expectedPlate))
			{
				utf16 = false;
				encoded = true;
				return true;
			}

			return false;
		}

		static bool TryRawMatchAt(uint64_t address, const char* expectedPlate, char out[8])
		{
			auto bytes = FrameWork::Memory::ReadBytes(address, 8);
			if (bytes.size() != 8)
				return false;

			std::memcpy(out, bytes.data(), 8);
			SanitizePlateBuffer(out);
			return PlatesMatchNormalized(out, expectedPlate);
		}

		static PlateWriteTarget FindMatchingPlateTarget(uintptr_t vehicle, const char* expectedPlate)
		{
			PlateWriteTarget target;
			char scratch[8] = {};

			const uint64_t vehicleOffsets[] = {
				Offsets::VehiclePlateText,
				0x928, 0x938, 0x970, 0x972, 0x9A0, 0x9B0,
				0xC90, 0x13C0, 0x1390, 0x13A0, 0x13B0,
			};

			for (uint64_t offset : vehicleOffsets)
			{
				if (!offset)
					continue;

				if (TryMatchPlateAt(vehicle + offset, expectedPlate, scratch, target.utf16, target.encoded))
				{
					target.address = vehicle + offset;
					return target;
				}
			}

			const uint64_t extension = FrameWork::Memory::ReadMemory<uint64_t>(vehicle + 0x1398);
			if (IsValidRemotePointer(extension))
			{
				for (uint64_t offset = 0x0; offset <= 0x100; offset += 0x8)
				{
					if (TryMatchPlateAt(extension + offset, expectedPlate, scratch, target.utf16, target.encoded))
					{
						target.address = extension + offset;
						return target;
					}
				}
			}

			uint8_t referenceEncoded[8] = {};
			EncodePlateBuffer(expectedPlate, referenceEncoded);
			if (const uint64_t encodedAddress = ScanVehicleForEncodedPattern(vehicle, referenceEncoded))
			{
				target.address = encodedAddress;
				target.encoded = true;
				return target;
			}

			for (uint64_t offset = 0x800; offset <= 0x1800; offset += 0x8)
			{
				if (TryMatchPlateAt(vehicle + offset, expectedPlate, scratch, target.utf16, target.encoded))
				{
					target.address = vehicle + offset;
					return target;
				}

				if (TryRawMatchAt(vehicle + offset, expectedPlate, scratch))
				{
					target.address = vehicle + offset;
					target.utf16 = false;
					return target;
				}
			}

			return target;
		}

		static PlateWriteTarget FindAnyValidPlateTarget(uintptr_t vehicle, char currentPlate[8])
		{
			PlateWriteTarget target;

			const uint64_t vehicleOffsets[] = {
				Offsets::VehiclePlateText,
				0x928, 0x938, 0x970, 0x972, 0x9A0, 0x9B0,
				0xC90, 0x13C0, 0x1390, 0x13A0, 0x13B0,
			};

			for (uint64_t offset : vehicleOffsets)
			{
				if (!offset)
					continue;

				if (ReadAsciiPlateAt(vehicle + offset, currentPlate))
				{
					target.address = vehicle + offset;
					return target;
				}

				if (ReadUtf16PlateAt(vehicle + offset, currentPlate))
				{
					target.address = vehicle + offset;
					target.utf16 = true;
					return target;
				}

				if (ReadAsciiPlateViaStringPointer(vehicle + offset, currentPlate))
				{
					target.address = vehicle + offset;
					return target;
				}

				uint8_t encodedBytes[8] = {};
				if (ReadEncodedPlateAt(vehicle + offset, encodedBytes, currentPlate))
				{
					target.address = vehicle + offset;
					target.encoded = true;
					return target;
				}
			}

			const uint64_t extension = FrameWork::Memory::ReadMemory<uint64_t>(vehicle + 0x1398);
			if (IsValidRemotePointer(extension))
			{
				for (uint64_t offset = 0x0; offset <= 0x100; offset += 0x8)
				{
					if (ReadAsciiPlateAt(extension + offset, currentPlate))
					{
						target.address = extension + offset;
						return target;
					}

					if (ReadUtf16PlateAt(extension + offset, currentPlate))
					{
						target.address = extension + offset;
						target.utf16 = true;
						return target;
					}

					if (ReadAsciiPlateViaStringPointer(extension + offset, currentPlate))
					{
						target.address = extension + offset;
						return target;
					}

					uint8_t encodedBytes[8] = {};
					if (ReadEncodedPlateAt(extension + offset, encodedBytes, currentPlate))
					{
						target.address = extension + offset;
						target.encoded = true;
						return target;
					}
				}
			}

			for (uint64_t offset = 0x400; offset <= 0x2000; offset += 0x4)
			{
				uint8_t encodedBytes[8] = {};
				if (ReadEncodedPlateAt(vehicle + offset, encodedBytes, currentPlate))
				{
					target.address = vehicle + offset;
					target.encoded = true;
					return target;
				}
			}

			return target;
		}

		static bool WritePlateBytes(uint64_t address, const void* data, size_t size)
		{
			if (!address || !data || !size)
				return false;

			return FrameWork::Memory::WriteProcessMemoryImpl(address, const_cast<void*>(data), size);
		}

		static bool WriteAsciiOrUtf16Plate(uint64_t address, const char* newPlate, bool isUtf16)
		{
			if (!address || !newPlate)
				return false;

			if (!isUtf16)
				return WritePlateBytes(address, newPlate, 8);

			wchar_t widePlate[8] = {};
			for (int i = 0; i < 8; ++i)
				widePlate[i] = static_cast<wchar_t>(newPlate[i]);

			return WritePlateBytes(address, widePlate, sizeof(widePlate));
		}

		static bool TryWriteAsciiAtValidatedOffset(uint64_t address, const char* referencePlate, const char* newPlate)
		{
			char existing[8] = {};

			if (ReadAsciiPlateAt(address, existing) && PlatesMatchNormalized(existing, referencePlate))
				return WriteAsciiOrUtf16Plate(address, newPlate, false);

			if (ReadUtf16PlateAt(address, existing) && PlatesMatchNormalized(existing, referencePlate))
				return WriteAsciiOrUtf16Plate(address, newPlate, true);

			if (ReadAsciiPlateViaStringPointer(address, existing) && PlatesMatchNormalized(existing, referencePlate))
			{
				const uint64_t pointer = FrameWork::Memory::ReadMemory<uint64_t>(address);
				char writeBuffer[9] = {};
				std::memcpy(writeBuffer, newPlate, 8);
				writeBuffer[8] = '\0';
				return WritePlateBytes(pointer, writeBuffer, 9);
			}

			return false;
		}

		static bool IsSafeAsciiPlateSlot(uint64_t address)
		{
			auto bytes = FrameWork::Memory::ReadBytes(address, 8);
			if (bytes.size() != 8)
				return false;

			uint64_t asPointer = 0;
			std::memcpy(&asPointer, bytes.data(), 8);
			if (IsValidRemotePointer(asPointer))
			{
				bool looksLikeAscii = true;
				for (uint8_t byte : bytes)
				{
					if (byte == '\0' || byte == ' ')
						continue;

					if (!((byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')))
					{
						looksLikeAscii = false;
						break;
					}
				}

				if (!looksLikeAscii)
					return false;
			}

			int alnum = 0;
			for (uint8_t byte : bytes)
			{
				if (byte == '\0' || byte == ' ')
					continue;

				if ((byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z'))
				{
					++alnum;
					continue;
				}

				return false;
			}

			return alnum >= 2;
		}

		static bool WriteEncodedPlateViaProbe(uintptr_t vehicle, const char* newPlate)
		{
			if (!vehicle || !newPlate)
				return false;

			uint8_t encodedPlate[8] = {};
			EncodePlateBuffer(newPlate, encodedPlate);

			const uint64_t drawHandlerOffsets[] = { 0x48, 0x40, 0x50, 0x58, 0x30 };
			const uint64_t shaderOffsets[] = { 0x18, 0x20, 0x28, 0x30, 0x38 };
			const uint64_t plateFieldOffsets[] = { 0x138, 0x130, 0x128, 0x140 };

			auto tryWriteAtField = [&](uint64_t baseAddress) -> bool
			{
				if (!IsValidRemotePointer(baseAddress))
					return false;

				for (uint64_t plateFieldOffset : plateFieldOffsets)
				{
					const uint64_t fieldAddress = baseAddress + plateFieldOffset;
					const uint64_t fieldValue = FrameWork::Memory::ReadMemory<uint64_t>(fieldAddress);

					if (Offsets::VehiclePlatePointerOnly)
					{
						if (IsValidRemotePointer(fieldValue) &&
							FrameWork::Memory::ReadBytes(fieldValue, 8).size() == 8 &&
							WritePlateBytes(fieldValue, encodedPlate, 8))
							return true;
					}
					else
					{
						if (WritePlateBytes(fieldAddress, encodedPlate, 8))
							return true;

						if (IsValidRemotePointer(fieldValue) && WritePlateBytes(fieldValue, encodedPlate, 8))
							return true;
					}
				}

				return false;
			};

			for (uint64_t drawHandlerOffset : drawHandlerOffsets)
			{
				const uint64_t drawHandler = FrameWork::Memory::ReadMemory<uint64_t>(vehicle + drawHandlerOffset);
				if (!IsValidRemotePointer(drawHandler))
					continue;

				for (uint64_t shaderOffset : shaderOffsets)
				{
					const uint64_t shaderEffect = FrameWork::Memory::ReadMemory<uint64_t>(drawHandler + shaderOffset);
					if (tryWriteAtField(shaderEffect))
						return true;
				}

				if (tryWriteAtField(drawHandler))
					return true;
			}

			const uint64_t directOffsets[] = { 0x20, 0x30, 0x38, 0x40, 0x48, 0x90, 0xA8, 0xB0 };
			for (uint64_t directOffset : directOffsets)
			{
				const uint64_t candidate = FrameWork::Memory::ReadMemory<uint64_t>(vehicle + directOffset);
				if (tryWriteAtField(candidate))
					return true;
			}

			return false;
		}

		static bool WriteAsciiPlateAnywhere(uintptr_t vehicle, const char* newPlate)
		{
			if (!vehicle || !newPlate)
				return false;

			const uint64_t knownOffsets[] = {
				Offsets::VehiclePlateText,
				0x928, 0x938, 0x970, 0x972, 0x9A0, 0x9B0,
				0x890, 0x898, 0x8A0, 0x8A8, 0x8B0, 0x8B8,
				0xC90, 0x13C0, 0x1390, 0x13A0, 0x13B0,
			};

			for (uint64_t offset : knownOffsets)
			{
				if (!offset)
					continue;

				const uint64_t address = vehicle + offset;
				if (IsSafeAsciiPlateSlot(address) && WriteAsciiOrUtf16Plate(address, newPlate, false))
					return true;

				char pointerPlate[8] = {};
				if (ReadAsciiPlateViaStringPointer(address, pointerPlate))
				{
					const uint64_t pointer = FrameWork::Memory::ReadMemory<uint64_t>(address);
					char writeBuffer[9] = {};
					std::memcpy(writeBuffer, newPlate, 8);
					writeBuffer[8] = '\0';
					if (WritePlateBytes(pointer, writeBuffer, 9))
						return true;
				}
			}

			for (uint64_t offset = 0x400; offset <= 0x2000; offset += 0x4)
			{
				const uint64_t address = vehicle + offset;
				if (IsSafeAsciiPlateSlot(address) && WriteAsciiOrUtf16Plate(address, newPlate, false))
					return true;
			}

			const uint64_t extension = FrameWork::Memory::ReadMemory<uint64_t>(vehicle + 0x1398);
			if (IsValidRemotePointer(extension))
			{
				for (uint64_t offset = 0x0; offset <= 0x200; offset += 0x4)
				{
					const uint64_t address = extension + offset;
					if (IsSafeAsciiPlateSlot(address) && WriteAsciiOrUtf16Plate(address, newPlate, false))
						return true;
				}
			}

			return false;
		}

		struct PlateWriteResult
		{
			bool visual = false;
			bool storage = false;
		};

		static uint64_t DiscoverPlateStorageByAnchor(uintptr_t vehicle, const char* anchorPlate, bool& encoded)
		{
			encoded = false;
			if (!vehicle || !anchorPlate || !anchorPlate[0])
				return 0;

			char normalizedAnchor[8] = {};
			NormalizePlateBuffer(anchorPlate, normalizedAnchor);

			uint8_t encodedAnchor[8] = {};
			EncodePlateBuffer(normalizedAnchor, encodedAnchor);

			auto matchesAnchorAscii = [&](uint64_t address) -> bool
			{
				char current[8] = {};
				if (ReadAsciiPlateAt(address, current) && PlatesMatchNormalized(current, normalizedAnchor))
					return true;

				if (ReadUtf16PlateAt(address, current) && PlatesMatchNormalized(current, normalizedAnchor))
					return true;

				return ReadAsciiPlateViaStringPointer(address, current) && PlatesMatchNormalized(current, normalizedAnchor);
			};

			auto matchesAnchorEncoded = [&](uint64_t address) -> bool
			{
				uint8_t encodedBytes[8] = {};
				char decoded[8] = {};
				return ReadEncodedPlateAt(address, encodedBytes, decoded) && PlatesMatchNormalized(decoded, normalizedAnchor);
			};

			for (uint64_t offset = 0; offset <= 0x3000; ++offset)
			{
				const uint64_t address = vehicle + offset;
				if (matchesAnchorAscii(address))
					return address;

				if (matchesAnchorEncoded(address))
				{
					encoded = true;
					return address;
				}
			}

			for (uint64_t offset = 0; offset <= 0x2000; offset += 0x8)
			{
				const uint64_t pointer = FrameWork::Memory::ReadMemory<uint64_t>(vehicle + offset);
				if (!IsValidRemotePointer(pointer))
					continue;

				for (uint64_t subOffset = 0; subOffset <= 0x400; ++subOffset)
				{
					const uint64_t address = pointer + subOffset;
					if (matchesAnchorAscii(address))
						return address;

					if (matchesAnchorEncoded(address))
					{
						encoded = true;
						return address;
					}
				}
			}

			for (uint64_t offset = 0x400; offset <= 0x2000; ++offset)
			{
				auto bytes = FrameWork::Memory::ReadBytes(vehicle + offset, 8);
				if (bytes.size() == 8 && std::memcmp(bytes.data(), encodedAnchor, 8) == 0)
				{
					encoded = true;
					return vehicle + offset;
				}
			}

			return 0;
		}

		static bool WritePlateToTarget(const PlateWriteTarget& target, const char* newPlate)
		{
			if (!target.address || !newPlate)
				return false;

			char normalizedPlate[8] = {};
			NormalizePlateBuffer(newPlate, normalizedPlate);

			if (target.encoded)
			{
				uint8_t encodedPlate[8] = {};
				EncodePlateBuffer(normalizedPlate, encodedPlate);
				return WritePlateBytes(target.address, encodedPlate, 8);
			}

			char pointerPlate[8] = {};
			if (ReadAsciiPlateViaStringPointer(target.address, pointerPlate))
			{
				const uint64_t pointer = FrameWork::Memory::ReadMemory<uint64_t>(target.address);
				char writeBuffer[9] = {};
				std::memcpy(writeBuffer, normalizedPlate, 8);
				writeBuffer[8] = '\0';
				return WritePlateBytes(pointer, writeBuffer, 9);
			}

			return WriteAsciiOrUtf16Plate(target.address, normalizedPlate, target.utf16);
		}

		static bool WriteEncodedPlateViaAnchor(uintptr_t vehicle, const char* newPlate, const char* scanAnchor)
		{
			if (!vehicle || !newPlate || !scanAnchor || !scanAnchor[0])
				return false;

			char normalizedAnchor[8] = {};
			NormalizePlateBuffer(scanAnchor, normalizedAnchor);

			const uint64_t shaderEffect = ResolveVehicleShaderEffect(vehicle, normalizedAnchor);
			if (!shaderEffect)
				return false;

			char verifyPlate[8] = {};
			if (!ReadCurrentPlateFromShader(shaderEffect, verifyPlate))
				return false;

			if (!PlatesMatchNormalized(verifyPlate, normalizedAnchor))
				return false;

			const uint64_t writeAddress = FindEncodedPlateWriteAddress(shaderEffect);
			if (!writeAddress)
				return false;

			char normalizedPlate[8] = {};
			NormalizePlateBuffer(newPlate, normalizedPlate);

			uint8_t encodedPlate[8] = {};
			EncodePlateBuffer(normalizedPlate, encodedPlate);
			return WritePlateBytes(writeAddress, encodedPlate, 8);
		}

		PlateWriteResult SetPlateTextEx(const char* text, const char* scanAnchor = nullptr)
		{
			PlateWriteResult result;
			if (!this || !text)
				return result;

			char newPlate[8] = {};
			NormalizePlateBuffer(text, newPlate);

			if (Offsets::VehiclePlatePointerOnly)
			{
				if (!scanAnchor || !scanAnchor[0])
					return result;

				char normalizedAnchor[8] = {};
				NormalizePlateBuffer(scanAnchor, normalizedAnchor);

				result.visual = WriteEncodedPlateViaAnchor((uintptr_t)this, newPlate, normalizedAnchor);

				const PlateWriteTarget storageTarget =
					FindMatchingPlateTarget(reinterpret_cast<uintptr_t>(this), normalizedAnchor);
				if (storageTarget.address)
					result.storage = WritePlateToTarget(storageTarget, newPlate);

				return result;
			}

			bool anchorEncoded = false;
			uint64_t discoveredAddress = DiscoverPlateStorageByAnchor((uintptr_t)this, scanAnchor, anchorEncoded);
			if (!discoveredAddress && Offsets::VehiclePlateText)
			{
				const uint64_t offsetAddress = (uintptr_t)this + Offsets::VehiclePlateText;
				char existing[8] = {};
				if (ReadAsciiPlateAt(offsetAddress, existing))
					discoveredAddress = offsetAddress;
			}

			if (scanAnchor && scanAnchor[0])
				result.visual = WriteEncodedPlateViaAnchor((uintptr_t)this, newPlate, scanAnchor);

			if (discoveredAddress)
			{
				PlateWriteTarget storageTarget;
				storageTarget.address = discoveredAddress;
				storageTarget.encoded = anchorEncoded;
				result.storage = WritePlateToTarget(storageTarget, newPlate);
			}

			if (!result.storage)
				result.storage = WriteAsciiPlateAnywhere((uintptr_t)this, newPlate);

			return result;
		}

		bool MatchesPlateText(const char* expected) const
		{
			if (!this || !expected || !expected[0])
				return false;

			char normalizedExpected[8] = {};
			NormalizePlateBuffer(expected, normalizedExpected);
			return FindMatchingPlateTarget(reinterpret_cast<uintptr_t>(this), normalizedExpected).address != 0;
		}

		bool SetPlateText(const char* text, const char* scanAnchor = nullptr)
		{
			const PlateWriteResult result = SetPlateTextEx(text, scanAnchor);
			return result.visual || result.storage;
		}
	};

	class CPlayerInfo
	{
	public:
		int GetPlayerID()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<int>(this + Offsets::PlayerNetID);
		}
	};

	class CPed
	{
	public:
		CPlayerInfo* GetPlayerInfo()
		{
			if (!this)
				return 0;

			return (CPlayerInfo*)FrameWork::Memory::ReadMemory<uint64_t>(this + Offsets::PlayerInfo);
		}

		uint32_t GetPedType()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<uint32_t>(this + Offsets::EntityType);
		}

		uint64_t GetNavigation()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<uint64_t>(this + 0x30);
		}

		uint64_t GetModelInfo()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<uint64_t>(this + 0x20);
		}

		Vector3D GetCoordinate()
		{
			if (!this)
				return Vector3D{ 0,0,0 };

			return FrameWork::Memory::ReadMemory<Vector3D>(this + 0x90);
		}

		CWeaponManager* GetWeaponManager()
		{
			if (!this)
				return 0;

			return (CWeaponManager*)FrameWork::Memory::ReadMemory<uint64_t>(this + Offsets::WeaponManager);
		}

		CVehicle* GetLastVehicle()
		{
			if (!this)
				return 0;

			return (CVehicle*)FrameWork::Memory::ReadMemory<uint64_t>(this + Offsets::LastVehicle);
		}

		CVehicle* GetCurrentVehicle()
		{
			if (!this)
				return 0;

			auto readAt = [this](uint64_t offset) -> CVehicle*
			{
				if (!offset)
					return 0;

				return (CVehicle*)FrameWork::Memory::ReadMemory<uint64_t>((uintptr_t)this + offset);
			};

			if (CVehicle* vehicle = readAt(Offsets::CurrentVehicle))
				return vehicle;

			if (CVehicle* vehicle = readAt(0xD10))
				return vehicle;

			if (CVehicle* vehicle = readAt(0xD30))
				return vehicle;

			return GetLastVehicle();
		}

		bool IsVisible()
		{
			if (!this)
				return false;

			BYTE VisibilityFlag = FrameWork::Memory::ReadMemory<BYTE>(this + Offsets::VisibleFlag);

			if (VisibilityFlag == 36 || VisibilityFlag == 0 || VisibilityFlag == 4)
				return false;

			return true;
		}

		float GetArmor()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<float>(this + Offsets::Armor);
		}

		float GetHealth()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<float>(this + 0x280);
		}

		float GetMaxHealth()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<float>(this + Offsets::MaxHealth);
		}

		void SetHealth(float NewHealth)
		{
			if (!this)
				return;

			FrameWork::Memory::WriteMemory(this + 0x280, NewHealth);
		}

		bool IsNPC()
		{
			uint32_t PedType = GetPedType();

			if (!PedType)
				return false;

			PedType = PedType << 11 >> 25;

			if (PedType != 2)
				return true;

			return false;
		}

		void SetConfigFlag(ePedConfigFlags Flag, bool Value)
		{
			int v1 = (int)Flag;
			if (!this || v1 >= 0x1CA)
				return;

			auto v2 = 1 << (v1 & 0x1F);
			auto v3 = v1 >> 5;
			auto v4 = (uint64_t)this + 4 * v3 + Offsets::ConfigFlags;
			auto v5 = FrameWork::Memory::ReadMemory<long>(v4);

			if (Value != ((v2 & v5) != 0))
			{
				auto v6 = v2 & (v5 ^ -(uint8_t)(Value ? 1 : 0));
				v5 ^= v6;

				FrameWork::Memory::WriteMemory(v4, v5);
			}
		}

		bool HasConfigFlag(ePedConfigFlags Flag)
		{
			int v1 = (int)Flag;
			if (!this || v1 >= 0x1CA)
				return false;

			auto v2 = 1 << (v1 & 0x1F);
			auto v3 = v1 >> 5;
			auto v4 = (uint64_t)this + 4 * v3 + Offsets::ConfigFlags;
			auto v5 = FrameWork::Memory::ReadMemory<long>(v4);

			return (v2 & v5) != 0;
		}
	};

	class CVehicleList
	{
	public:
		CVehicle* Vehicle(int Index)
		{
			if (!this)
				return 0;

			return (CVehicle*)FrameWork::Memory::ReadMemory<uint64_t>(this + (Index * 0x10));
		}
	};

	class CVehicleInterface
	{
	public:

		CVehicleList* VehicleList()
		{
			if (!this)
				return 0;

			return (CVehicleList*)FrameWork::Memory::ReadMemory<uint64_t>(this + 0x180);
		}

		uint64_t VehicleMaximum()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<uint64_t>(this + 0x188);
		}

		uint64_t VehiclesAtList()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<uint64_t>(this + 0x190);
		}
	};

	class CPedList
	{
	public:
		CPed* Ped(int Index)
		{
			if (!this)
				return 0;

			return (CPed*)FrameWork::Memory::ReadMemory<uint64_t>(this + (Index * 0x10));
		}
	};

	class CPedInterface
	{
	public:
		CPedList* PedList()
		{
			if (!this)
				return 0;

			return (CPedList*)FrameWork::Memory::ReadMemory<uint64_t>(this + 0x100);
		}

		uint64_t PedMaximum()
		{
			if (!this)
				return 0;

			return FrameWork::Memory::ReadMemory<uint64_t>(this + 0x108);
		}
	};

	class CReplayInterface
	{
	public:
		CVehicleInterface* VehicleInterface()
		{
			if (!this)
				return 0;

			return (CVehicleInterface*)FrameWork::Memory::ReadMemory<uint64_t>(this + 0x10);
		}

		CPedInterface* PedInterface()
		{
			if (!this)
				return 0;

			return (CPedInterface*)FrameWork::Memory::ReadMemory<uint64_t>(this + 0x18);
		}
	};

	class CCamFollowPedCamera
	{
	public:
		Vector3D GetViewAngles()
		{
			if (!this)
				return Vector3D(0, 0, 0);

			return FrameWork::Memory::ReadMemory<Vector3D>(this + 0x40);
		}

		void SetViewAngles(Vector3D Angles)
		{
			if (!this)
				return;

			FrameWork::Memory::WriteMemory(this + 0x40, Angles);
		}

		Vector3D GetCrosshairPosition()
		{
			if (!this)
				return Vector3D(0, 0, 0);

			return FrameWork::Memory::ReadMemory<Vector3D>(this + 0x60);
		}

		void SetCrosshairPosition(Vector3D Position)
		{
			if (!this)
				return;

			FrameWork::Memory::WriteMemory(this + 0x60, Position);
		}

		Vector3D GetThirdpersonViewAngles()
		{
			if (!this)
				return Vector3D(0, 0, 0);

			return FrameWork::Memory::ReadMemory<Vector3D>(this + 0x3d0);
		}

		void SetThirdpersonViewAngles(Vector3D ViewAngles)
		{
			if (!this)
				return;

			FrameWork::Memory::WriteMemory(this + 0x3d0, ViewAngles);
		}

		Vector3D GetCameraPosition()
		{
			if (!this)
				return Vector3D(0, 0, 0);

			return FrameWork::Memory::ReadMemory<Vector3D>(this + 0x3F0);
		}

		void SetCameraPosition(Vector3D NewPosition)
		{
			if (!this)
				return;

			FrameWork::Memory::WriteMemory(this + 0x3F0, NewPosition);
		}
	};

	class CCamGameplayDirector
	{
	public:
		CCamFollowPedCamera* GetFollowPedCamera()
		{
			if (!this)
				return 0;

			return (CCamFollowPedCamera*)FrameWork::Memory::ReadMemory<uint64_t>(this + 0x3C0);
		}
	};

	class CWorld
	{
	public:
		CPed* LocalPlayer()
		{
			if (!this)
				return 0;

			return (CPed*)FrameWork::Memory::ReadMemory<uint64_t>(this + 0x8);
		}
	};
}