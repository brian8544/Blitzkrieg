#ifndef __AI_CONSTS_H__
#define __AI_CONSTS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIConsts
{
	// макс. размер карты, макс. длина пути, макс. длина маленького пути, макс. 
	// кол-во ячеек, занятых юнитом, макс. кол-во юнитов, размер AI тайла,
	// максимальное количество дипломатических сторон
	static constexpr int MAX_MAP_SIZE = 1024;
	static constexpr int INFINITY_PATH_LIMIT = 5000;
	static constexpr int MAX_LENGTH_OF_SMALL_PATH = 20;
	static constexpr int MAX_NUMBER_OF_UNITS = 3000;
	static constexpr int AI_SEGMENT_DURATION = 50;
	static constexpr int TILE_SIZE = 32;
	static constexpr int MAX_NUM_OF_PLAYERS = 16;
	static constexpr int VIS_POWER = 7;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AI_CONSTS_H__
