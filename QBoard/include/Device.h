/**
 * @file Device.h
 * @brief
 * @author Xu7 (xuqi.jx@gmail.com)
 * @version 1.0
 * @date 2022-04-05
 *
 * @copyright Copyright (c) 2022  XX
 *
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2022-04-05 <td>1.0     <td>xu7     <td>内容
 * </table>
 */
#pragma once

class Device
{
  public:
    /**
     * @brief
     */
    virtual void setup() = 0;

    /**
     * @brief
     */
    virtual void loop() = 0;
};