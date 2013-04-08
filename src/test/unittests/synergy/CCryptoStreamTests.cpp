/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2013 Bolton Software Ltd.
 * 
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file COPYING that should have accompanied this file.
 * 
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>
#include "CCryptoStream.h"
#include "CMockStream.h"
#include "CMockEventQueue.h"
#include "CPacketStreamFilter.h"

using ::testing::_;
using ::testing::Invoke;

using namespace std;

UInt8 g_write_buffer[4];
void write_mockWrite(const void* in, UInt32 n);

UInt8 g_read_buffer[4];
UInt8 read_mockRead(void* out, UInt32 n);

UInt8 g_write4Read1_buffer[4];
UInt32 g_write4Read1_bufferIndex = 0;
void write4Read1_mockWrite(const void* in, UInt32 n);
UInt8 write4Read1_mockRead(void* out, UInt32 n);

UInt8 g_write1Read4_buffer[4];
UInt32 g_write1Read4_bufferIndex = 0;
void write1Read4_mockWrite(const void* in, UInt32 n);
UInt8 write1Read4_mockRead(void* out, UInt32 n);

UInt8 g_readWriteIvChanged_buffer[4];
UInt32 g_readWriteIvChangeTrigger_writeBufferIndex = 0;
UInt32 g_readWriteIvChangeTrigger_readBufferIndex = 0;
void readWriteIvChanged_mockWrite(const void* in, UInt32 n);
UInt8 readWriteIvChanged_mockRead(void* out, UInt32 n);

UInt8 g_readWriteIvChangeTrigger_buffer[4 + 4 + 16]; // abcd, DCIV, 16-byte IV
void readWriteIvChangeTrigger_mockWrite(const void* in, UInt32 n);
UInt8 readWriteIvChangeTrigger_mockRead(void* out, UInt32 n);

const byte g_key[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; // +\0, 32-byte/256-bit key.
const byte g_iv[] = "bbbbbbbbbbbbbb"; // +\0, AES block size = 16

TEST(CCryptoTests, write)
{
	const UInt32 size = 4;
	UInt8* buffer = new UInt8[size];
	buffer[0] = 'D';
	buffer[1] = 'K';
	buffer[2] = 'D';
	buffer[3] = 'N';
	
	CMockEventQueue eventQueue;
	CMockStream innerStream(eventQueue);
	
	ON_CALL(innerStream, write(_, _)).WillByDefault(Invoke(write_mockWrite));
	EXPECT_CALL(innerStream, write(_, _)).Times(1);
	EXPECT_CALL(innerStream, getEventTarget()).Times(3);
	EXPECT_CALL(eventQueue, removeHandlers(_)).Times(1);
	EXPECT_CALL(eventQueue, adoptHandler(_, _, _)).Times(1);
	EXPECT_CALL(eventQueue, removeHandler(_, _)).Times(1);

	CCryptoStream cs(eventQueue, &innerStream, false);
	cs.setKeyWithIv(g_key, sizeof(g_key), g_iv);
	cs.write(buffer, size);
	
	EXPECT_EQ(254, g_write_buffer[0]);
	EXPECT_EQ(44, g_write_buffer[1]);
	EXPECT_EQ(187, g_write_buffer[2]);
	EXPECT_EQ(253, g_write_buffer[3]);
}

TEST(CCryptoTests, read)
{
	CMockEventQueue eventQueue;
	CMockStream innerStream(eventQueue);
	
	ON_CALL(innerStream, read(_, _)).WillByDefault(Invoke(read_mockRead));
	EXPECT_CALL(innerStream, read(_, _)).Times(1);
	EXPECT_CALL(innerStream, getEventTarget()).Times(3);
	EXPECT_CALL(eventQueue, removeHandlers(_)).Times(1);
	EXPECT_CALL(eventQueue, adoptHandler(_, _, _)).Times(1);
	EXPECT_CALL(eventQueue, removeHandler(_, _)).Times(1);

	CCryptoStream cs(eventQueue, &innerStream, false);
	cs.setKeyWithIv(g_key, sizeof(g_key), g_iv);
	
	g_read_buffer[0] = 254;
	g_read_buffer[1] = 44;
	g_read_buffer[2] = 187;
	g_read_buffer[3] = 253;

	const UInt32 size = 4;
	UInt8* buffer = new UInt8[size];
	cs.read(buffer, size);

	EXPECT_EQ('D', buffer[0]);
	EXPECT_EQ('K', buffer[1]);
	EXPECT_EQ('D', buffer[2]);
	EXPECT_EQ('N', buffer[3]);
}

TEST(CCryptoTests, write4Read1)
{
	CMockEventQueue eventQueue;
	CMockStream innerStream(eventQueue);
	
	ON_CALL(innerStream, write(_, _)).WillByDefault(Invoke(write4Read1_mockWrite));
	ON_CALL(innerStream, read(_, _)).WillByDefault(Invoke(write4Read1_mockRead));
	EXPECT_CALL(innerStream, write(_, _)).Times(4);
	EXPECT_CALL(innerStream, read(_, _)).Times(1);
	EXPECT_CALL(innerStream, getEventTarget()).Times(6);
	EXPECT_CALL(eventQueue, removeHandlers(_)).Times(2);
	EXPECT_CALL(eventQueue, adoptHandler(_, _, _)).Times(2);
	EXPECT_CALL(eventQueue, removeHandler(_, _)).Times(2);

	CCryptoStream cs1(eventQueue, &innerStream, false);
	cs1.setKeyWithIv(g_key, sizeof(g_key), g_iv);
	
	cs1.write("a", 1);
	cs1.write("b", 1);
	cs1.write("c", 1);
	cs1.write("d", 1);

	CCryptoStream cs2(eventQueue, &innerStream, false);
	cs2.setKeyWithIv(g_key, sizeof(g_key), g_iv);
	
	UInt8 buffer[4];
	cs2.read(buffer, 4);
	
	EXPECT_EQ('a', buffer[0]);
	EXPECT_EQ('b', buffer[1]);
	EXPECT_EQ('c', buffer[2]);
	EXPECT_EQ('d', buffer[3]);
}

TEST(CCryptoTests, write1Read4)
{
	CMockEventQueue eventQueue;
	CMockStream innerStream(eventQueue);
	
	ON_CALL(innerStream, write(_, _)).WillByDefault(Invoke(write1Read4_mockWrite));
	ON_CALL(innerStream, read(_, _)).WillByDefault(Invoke(write1Read4_mockRead));
	EXPECT_CALL(innerStream, write(_, _)).Times(1);
	EXPECT_CALL(innerStream, read(_, _)).Times(4);
	EXPECT_CALL(innerStream, getEventTarget()).Times(6);
	EXPECT_CALL(eventQueue, removeHandlers(_)).Times(2);
	EXPECT_CALL(eventQueue, adoptHandler(_, _, _)).Times(2);
	EXPECT_CALL(eventQueue, removeHandler(_, _)).Times(2);

	CCryptoStream cs1(eventQueue, &innerStream, false);
	cs1.setKeyWithIv(g_key, sizeof(g_key), g_iv);

	UInt8 bufferIn[4];
	bufferIn[0] = 'a';
	bufferIn[1] = 'b';
	bufferIn[2] = 'c';
	bufferIn[3] = 'd';
	cs1.write(bufferIn, 4);
	
	CCryptoStream cs2(eventQueue, &innerStream, false);
	cs2.setKeyWithIv(g_key, sizeof(g_key), g_iv);

	UInt8 bufferOut[4];
	cs2.read(&bufferOut[0], 1);
	cs2.read(&bufferOut[1], 1);
	cs2.read(&bufferOut[2], 1);
	cs2.read(&bufferOut[3], 1);
	
	EXPECT_EQ('a', bufferOut[0]);
	EXPECT_EQ('b', bufferOut[1]);
	EXPECT_EQ('c', bufferOut[2]);
	EXPECT_EQ('d', bufferOut[3]);
}

TEST(CCryptoTests, readWriteIvChanged)
{
	CMockEventQueue eventQueue;
	CMockStream innerStream(eventQueue);
	
	ON_CALL(innerStream, write(_, _)).WillByDefault(Invoke(readWriteIvChanged_mockWrite));
	ON_CALL(innerStream, read(_, _)).WillByDefault(Invoke(readWriteIvChanged_mockRead));
	EXPECT_CALL(innerStream, write(_, _)).Times(2);
	EXPECT_CALL(innerStream, read(_, _)).Times(2);
	EXPECT_CALL(innerStream, getEventTarget()).Times(6);
	EXPECT_CALL(eventQueue, removeHandlers(_)).Times(2);
	EXPECT_CALL(eventQueue, adoptHandler(_, _, _)).Times(2);
	EXPECT_CALL(eventQueue, removeHandler(_, _)).Times(2);
	
	const byte iv1[] = "bbbbbbbbbbbbbbb";
	const byte iv2[] = "ccccccccccccccc";

	CCryptoStream cs1(eventQueue, &innerStream, false);
	cs1.setKeyWithIv(g_key, sizeof(g_key), iv1);
	
	UInt8 bufferIn[4];
	bufferIn[0] = 'a';
	bufferIn[1] = 'b';
	bufferIn[2] = 'c';
	bufferIn[3] = 'd';
	cs1.write(bufferIn, 4);
	
	CCryptoStream cs2(eventQueue, &innerStream, false);
	cs2.setKeyWithIv(g_key, sizeof(g_key), iv2);

	UInt8 bufferOut[4];
	cs2.read(bufferOut, 4);
	
	// assert that the values cannot be decrypted, since the second crypto
	// class instance is using a different IV.
	EXPECT_NE('a', bufferOut[0]);
	EXPECT_NE('b', bufferOut[1]);
	EXPECT_NE('c', bufferOut[2]);
	EXPECT_NE('d', bufferOut[3]);

	// generate a new IV and copy it to the second crypto class, and
	// ensure that the new IV is used.
	byte iv[CRYPTO_IV_SIZE];
	cs1.newIv(iv);
	cs2.setIv(iv);

	cs1.write(bufferIn, 4);
	cs2.read(bufferOut, 4);

	EXPECT_EQ('a', bufferOut[0]);
	EXPECT_EQ('b', bufferOut[1]);
	EXPECT_EQ('c', bufferOut[2]);
	EXPECT_EQ('d', bufferOut[3]);
}

void
write_mockWrite(const void* in, UInt32 n)
{
	memcpy(g_write_buffer, in, n);
}

UInt8
read_mockRead(void* out, UInt32 n)
{
	memcpy(out, g_read_buffer, n);
	return n;
}

void
write4Read1_mockWrite(const void* in, UInt32 n)
{
	UInt8* buffer = static_cast<UInt8*>(const_cast<void*>(in));
	g_write4Read1_buffer[g_write4Read1_bufferIndex++] = buffer[0];
}

UInt8
write4Read1_mockRead(void* out, UInt32 n)
{
	memcpy(out, g_write4Read1_buffer, n);
	return n;
}

void
write1Read4_mockWrite(const void* in, UInt32 n)
{
	memcpy(g_write1Read4_buffer, in, n);
}

UInt8
write1Read4_mockRead(void* out, UInt32 n)
{
	UInt8* buffer = static_cast<UInt8*>(out);
	buffer[0] = g_write1Read4_buffer[g_write1Read4_bufferIndex++];
	return 1;
}

void
readWriteIvChanged_mockWrite(const void* in, UInt32 n)
{
	memcpy(g_readWriteIvChanged_buffer, in, n);
}

UInt8
readWriteIvChanged_mockRead(void* out, UInt32 n)
{
	memcpy(out, g_readWriteIvChanged_buffer, n);
	return n;
}

// TODO: macro?

void
readWriteIvChangeTrigger_mockWrite(const void* in, UInt32 n)
{
	assert(g_readWriteIvChangeTrigger_writeBufferIndex <= sizeof(g_readWriteIvChangeTrigger_buffer));
	memcpy(&g_readWriteIvChangeTrigger_buffer[g_readWriteIvChangeTrigger_writeBufferIndex], in, n);
	g_readWriteIvChangeTrigger_writeBufferIndex += n;
}

UInt8
readWriteIvChangeTrigger_mockRead(void* out, UInt32 n)
{
	assert(g_readWriteIvChangeTrigger_readBufferIndex <= sizeof(g_readWriteIvChangeTrigger_buffer));
	memcpy(out, &g_readWriteIvChangeTrigger_buffer[g_readWriteIvChangeTrigger_readBufferIndex], n);
	g_readWriteIvChangeTrigger_readBufferIndex += n;
	return n;
}
